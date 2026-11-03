// engine.go —— 引擎子进程 + JSON Lines 协议（stdin/stdout）
package main

import (
	"bufio"
	"encoding/json"
	"io"
	"os"
	"os/exec"
	"path/filepath"
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
}

var eng = &Engine{}

// engineCandidates 引擎可执行文件的候选位置（先找同目录 engine 子目录，再找同目录）
func engineCandidates() []string {
	var out []string
	exe, err := os.Executable()
	if err == nil {
		dir := filepath.Dir(exe)
		out = append(out,
			filepath.Join(dir, "engine", "flare.exe"),
			filepath.Join(dir, "flare-engine.exe"),
			filepath.Join(dir, "flare.exe"),
		)
	}
	return out
}

func (e *Engine) Start() error {
	var path string
	for _, c := range engineCandidates() {
		if _, err := os.Stat(c); err == nil {
			// 跳过自身
			if self, err2 := os.Executable(); err2 == nil && filepath.Base(self) == filepath.Base(c) {
				if filepath.Dir(self) == filepath.Dir(c) {
					continue
				}
			}
			path = c
			break
		}
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
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
	in, err := cmd.StdinPipe()
	if err != nil {
		return err
	}
	out, err := cmd.StdoutPipe()
	if err != nil {
		return err
	}
	cmd.Stderr = nil
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
