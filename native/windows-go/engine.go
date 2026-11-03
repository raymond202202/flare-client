// engine.go —— 引擎子进程 + JSON Lines 协议（stdin/stdout）
package main

import (
	"bufio"
	"encoding/json"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"syscall"
)

// 自定消息：读线程收到一行 → 入队 → PostMessage 唤醒 UI 线程
const wmEngineEvent = WM_APP + 1
const wmEngineDead = WM_APP + 2

type Engine struct {
	cmd   *exec.Cmd
	stdin io.WriteCloser
	wmu   sync.Mutex

	hwnd uintptr

	qmu sync.Mutex
	q   []map[string]any

	Alive   bool
	Version string
	ExePath string

	errTail *tailBuf
}

// tailBuf 只保留最近 4KB 的 stderr，引擎异常退出时用于诊断
type tailBuf struct {
	mu  sync.Mutex
	buf []byte
}

func (t *tailBuf) Write(p []byte) (int, error) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.buf = append(t.buf, p...)
	if len(t.buf) > 4096 {
		t.buf = t.buf[len(t.buf)-4096:]
	}
	return len(p), nil
}

func (t *tailBuf) String() string {
	t.mu.Lock()
	defer t.mu.Unlock()
	return string(t.buf)
}

var eng = &Engine{}

// engineCandidates 引擎可执行文件的候选位置（同目录 engine 子目录 / 同目录改名）
//
// 注意：绝对不能把「同目录的 flare.exe」当候选——Windows 文件名不区分大小写，
// 而外壳自己就叫 Flare.exe，os.Stat 会命中外壳自身，导致反复启动自己。
// 这里只认两个不会与外壳冲突的位置，并用 EqualFold 兜底排除自身。
func engineCandidates() []string {
	var out []string
	exe, err := os.Executable()
	if err != nil {
		return out
	}
	dir := filepath.Dir(exe)

	self := filepath.Clean(exe)
	if abs, err := filepath.Abs(exe); err == nil {
		self = filepath.Clean(abs)
	}

	for _, c := range []string{
		filepath.Join(dir, "engine", "flare.exe"),
		filepath.Join(dir, "flare-engine.exe"),
	} {
		if abs, err := filepath.Abs(c); err == nil && strings.EqualFold(filepath.Clean(abs), self) {
			continue // 防御：绝不把自身当引擎拉起
		}
		out = append(out, c)
	}
	return out
}

func (e *Engine) Start() error {
	var path string
	for _, c := range engineCandidates() {
		if _, err := os.Stat(c); err != nil {
			continue
		}
		if self, err2 := os.Executable(); err2 == nil {
			sp, _ := filepath.Abs(self)
			cp, _ := filepath.Abs(c)
			if strings.EqualFold(filepath.Clean(sp), filepath.Clean(cp)) {
				continue // 防御：同名同路径视为自身
			}
		}
		path = c
		break
	}
	if path == "" {
		// 同目录没有引擎 → 释放内嵌引擎（单文件双击即用）
		rp, err := releaseEmbedded()
		if err != nil {
			return err
		}
		path = rp
	}
	e.ExePath = path

	cmd := exec.Command(path, "server")
	// CREATE_NO_WINDOW(0x08000000)：引擎是 console 子系统，
	// 只靠 HideWindow 在 Win7 上可能闪一下黑框，两个一起用彻底不分配控制台。
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true, CreationFlags: 0x08000000}
	in, err := cmd.StdinPipe()
	if err != nil {
		return err
	}
	out, err := cmd.StdoutPipe()
	if err != nil {
		return err
	}
	e.errTail = &tailBuf{}
	cmd.Stderr = e.errTail
	if err := cmd.Start(); err != nil {
		return err
	}
	e.cmd = cmd
	e.stdin = in
	e.Alive = true

	go e.readLoop(out)
	go func() {
		_ = cmd.Wait()
		e.Alive = false
		if e.hwnd != 0 {
			pPostMessageW.Call(e.hwnd, wmEngineDead, 0, 0)
		}
	}()

	// 握手
	e.Send(map[string]any{"type": "version"})
	e.Send(map[string]any{"type": "list_sessions"})
	return nil
}

func (e *Engine) readLoop(r io.Reader) {
	sc := bufio.NewScanner(r)
	sc.Buffer(make([]byte, 0, 64*1024), 8*1024*1024)
	for sc.Scan() {
		line := sc.Bytes()
		if len(line) == 0 {
			continue
		}
		var m map[string]any
		if err := json.Unmarshal(line, &m); err != nil {
			continue
		}
		if t, _ := m["type"].(string); t == "version" {
			if v, ok := m["engine"].(string); ok {
				e.Version = v
			}
		}
		e.qmu.Lock()
		e.q = append(e.q, m)
		e.qmu.Unlock()
		if e.hwnd != 0 {
			pPostMessageW.Call(e.hwnd, wmEngineEvent, 0, 0)
		}
	}
}

// Drain 由 UI 线程调用，取走所有待处理事件
func (e *Engine) Drain() []map[string]any {
	e.qmu.Lock()
	defer e.qmu.Unlock()
	if len(e.q) == 0 {
		return nil
	}
	out := e.q
	e.q = nil
	return out
}

func (e *Engine) Send(v any) {
	if e.stdin == nil {
		return
	}
	b, err := json.Marshal(v)
	if err != nil {
		return
	}
	b = append(b, '\n')
	e.wmu.Lock()
	_, _ = e.stdin.Write(b)
	e.wmu.Unlock()
}

func (e *Engine) Chat(text string) {
	e.Send(map[string]any{"type": "chat", "input": text, "sessionId": st.CurSession})
}

func (e *Engine) Cancel() {
	e.Send(map[string]any{"type": "cancel", "sessionId": st.CurSession})
}

func (e *Engine) LoadMessages(sid string) {
	e.Send(map[string]any{"type": "get_messages", "sessionId": sid})
}

func (e *Engine) NewSession() {
	e.Send(map[string]any{"type": "new_session"})
}

func (e *Engine) DeleteSession(sid string) {
	e.Send(map[string]any{"type": "delete_session", "sessionId": sid})
}

func (e *Engine) Stop() {
	if e.stdin != nil {
		e.Send(map[string]any{"type": "shutdown"})
		_ = e.stdin.Close()
	}
	if e.cmd != nil && e.cmd.Process != nil {
		_ = e.cmd.Process.Kill()
	}
}
