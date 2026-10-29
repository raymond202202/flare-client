# Flare 客户端渲染规范 v1.0（render-spec）

> **适用范围**：Linux（Qt Widgets，现行实现）、Windows（WPF，.NET FW 4.8）、macOS（SwiftUI）。
> **本文件的地位**：三平台壳子的**唯一视觉权威**。数值全部从现行 Qt 实现取证（`flare-client/src/`
> 下 `flametheme.h` 与 7 个面板的 `setStyleSheet` / 布局调用），不是设计稿臆测。
> 任何平台改样式，先改本文件，再改代码；不允许某平台单独「顺手调一下」。

---

## 0. 三条铁律（违反即视为不通过验收）

1. **无菜单栏**。macOS 也置空（`NSApp.mainMenu = nil`），Windows 用无 menu 的 `Window`。
   Qt 侧是 `setMenuBar(nullptr)`，这是用户铁律，不是可选项。
2. **引擎是子进程，壳子只画界面**。UI 不得直接读 `~/.flare/.env`、不得内嵌引擎逻辑；
   一律 `flare server` 子进程 + stdin/stdout JSON Lines（见 `protocol.md`）。
3. **浅色火焰主题**。禁止深色模式自动跟随；系统处于深色时本应用仍为浅色（`#ffffff` 窗口底）。

---

## 1. 颜色令牌

色值必须**逐字节一致**。语义名三端统一，任何平台不得自造色值。

### 1.1 火焰色阶（品牌色）

| 令牌 | 值 | 语义 |
|---|---|---|
| `red` | `#ef4444` | 火焰核心：标题强调、危险文字 |
| `orange` | `#f97316` | **主行动色**：主按钮底、面板标题、用户消息作者名、列表选中项底 |
| `amber` | `#f59e0b` | 过渡色：归档按钮文字 |
| `yellow` | `#fbbf24` | 亮黄：渐变终点 |
| `dark` | `#b45309` | 暗琥珀：状态栏次要链接默认态 |

### 1.2 背景与描边

| 令牌 | 值 | 语义 |
|---|---|---|
| `bgWindow` | `#ffffff` | 窗口底、弹窗底、次级按钮底、输入框（聊天输入、搜索框）底 |
| `bgSoft` | `#fffbf0` | **米白底**：列表容器、多行输出区、设置页输入框、工具卡片底（浅一档见 1.4） |
| `bgAlt` | `#fff3d6` | 浅琥珀底：交替行、工具卡片底 |
| `borderSoft` | `#fde6bf` | **浅橙边框**：所有 1px 描边、列表容器、次级按钮边框 |
| `borderSoft2` | `#fde3c6` | 版本面板 GroupBox 边框（比 `borderSoft` 略暖，仅此一处） |
| `hoverSoft` | `#ffedd0` | 列表项悬停底、次级按钮悬停底 |

### 1.3 文字

| 令牌 | 值 | 语义 |
|---|---|---|
| `textMain` | `#4a2e0d` | 深琥珀：正文、标签、弹窗文字、输入文字 |
| `textSoft` | `#8a6a3b` | 次级文字：表单说明、工具卡片正文（等宽） |
| `textMuted` | `#9a8f80` | 弱化文字：版本号副行、禁用按钮文字 |
| `textFaint` | `#b9ab99` | 最弱：版权/脚注行 |
| `textGray` | `#4b5563` | 会话搜索框输入文字（中性灰，与琥珀系刻意区分） |

### 1.4 交互态（由主行动色派生，不得自选）

| 用途 | 默认 | 悬停 | 按下 | 禁用 |
|---|---|---|---|---|
| 主按钮底 | `#f97316` | `#e0630f` | `#c04f08` | `#e5e0d8` |
| 版本面板主按钮底 | `#f97316` | `#ea580c` | `#c2410c` | `#e5e0d8` |
| 危险/停止按钮 | 底 `#ffffff` 字 `#ef4444` 边 `#fda4a4` | 底 `#fef2f2` | 底 `#fde8e8` | — |
| 状态栏链接文字 | `#b45309` | `#f97316` + 下划线 | `#c2410c` | — |

> 两套悬停值并存是历史事实（`chatwidget`/`sessionlist` 用 `#e0630f`+`#c04f08`，
> `versionpanel` 用 `#ea580c`+`#c2410c`）。原生壳**统一取表内对应行**：
> 聊天区与侧栏用左侧一套，版本面板用自己一行，不要互相污染。

### 1.5 火焰渐变（程序化，非固定色）

三端必须实现同一算法，用于欢迎词与呼吸效果：

```
flameColor(t):                        // t 任意实数，先取小数部分 tt = t - floor(t)
  if tt < 0.5: u = tt*2;              // 红 → 橙
      r = ef + (f9-ef)*u ; g = 44 + (73-44)*u ; b = 44 + (16-44)*u
  else:       u = (tt-0.5)*2;         // 橙 → 黄
      r = f9 + (fb-f9)*u ; g = 73 + (bf-73)*u ; b = 16 + (24-16)*u
  各通道四舍五入取整

breathColor(nowMs):                   // 呼吸相位，5s 周期
  t     = (nowMs mod 5000) / 5000
  phase = (sin(t * 4π) + 1) / 2
  return flameColor(phase)
```

逐字符渐变（欢迎词 "F L A R E"）：第 i 个字符取 `flameColor(i / (len-1))`，
`len<=1` 时取 `flameColor(0)`；空格与 emoji（码点 > `0xFFFF`）**不着色**。

---

## 2. 字体与字号阶梯

- **字族**：跟随系统 UI 字族（Qt 默认、WPF `Segoe UI`、SwiftUI `.system`）。不打包字体。
- **等宽字族**：系统等宽（Qt `monospace`、WPF `Consolas`、SwiftUI `.monospaced`），仅用于工具卡片正文。
- **字号是绝对 px/pt，不是相对缩放**：三端一律按表取整，不做 1.25 倍系统缩放的二次换算（DPI 缩放由系统层处理）。

| 用途 | 字号 | 字重 |
|---|---|---|
| 版权/脚注（`textFaint`） | 11 | Regular |
| 状态栏链接、版本副行、设置页说明、工具卡片正文 | 12 | Regular（链接悬停加下划线） |
| 列表项、搜索框、面板内按钮、表单说明 | 13 | Regular（面板标题除外） |
| 聊天输出区、输入框、发送/停止按钮、弹窗正文 | 14 | Regular |
| 面板标题（记忆/技能/会话标题） | 15 | **600 / SemiBold** |
| 版本面板段落标题 | 15 | **Bold** |
| 聊天区大标题「Flare 对话」 | 16 | **600 / SemiBold** |
| 设置页标题 | 18 | **600 / SemiBold** |

所有标题类文字颜色为 `orange`；正文为 `textMain`；说明类为 `textSoft`。

---

## 3. 布局几何

### 3.1 主窗口

| 项 | 值 |
|---|---|
| 窗口标题 | `Flare` |
| 默认尺寸 | `1024 × 700`（`MainWindow` 构造内先 `resize(980,640)`，`main.cpp` 随后 `resize(1024,700)` 覆盖——以 1024×700 为准） |
| 最小尺寸 | `820 × 600`（**硬下限**，低于此不缩，允许滚动条） |
| 结构 | 水平两栏：`侧栏（固定 220） | 聊天区（占满剩余）` |
| 侧栏 | 固定宽 `220`，**不可折叠、不可拖拽变宽**（Qt `setFixedWidth` + `setCollapsible(false)`） |
| 拉伸因子 | 侧栏 `0`、聊天区 `1` |
| 菜单栏 | 无 |
| 状态栏 | 常显：左「Flare 引擎已连接 / 启动失败」消息；右两个永久控件 ①`🤖 模型加载中…` → 加载后 `🤖 <model> · <provider>` ②版本链接 `客户端 vX · 内核 flare vY` |

> 状态栏两个版本号来源不同（编译期常量 vs 运行时 `version` 帧），**不得合并成一个数字**；
> 内核版本未到时显示占位，不要显示 0.0.0。

### 3.2 侧栏（会话列表）

纵向顺序（间距 8，外边距 8/8/8/8）：

1. 标题 `会话`（15 / SemiBold / `orange`）
2. 搜索框（单行，高约 30，圆角 6，底 `#ffffff`，边 `borderSoft`，focus 边 `orange`，文字 `textGray` 13px）
3. 会话列表（**弹性占满**，底 `bgSoft`，边 `borderSoft`，圆角 8，内边距 4，无 outline）
   - 列表项：内边距 `8 × 6`，圆角 6，项间 `1`
   - 悬停：底 `hoverSoft`
   - 选中：底 `orange`、文字纯白
4. 按钮行 1（间距 6）：`＋ 新建`（主按钮 13px，底 `orange`，圆角 6，内边距 `6 × 10`）｜`删除`（次级按钮：底 `#ffffff`、字 `orange`、边 `borderSoft`、悬停底 `hoverSoft`）
5. 按钮行 2：`🧠 记忆`（次级）｜`🔧 技能`（次级）
6. 按钮行 3：`📦 归档`（次级但文字 `amber` `#f59e0b`）｜`⚙️ 设置`（次级）

按钮在各行等宽分配（`stretch=1`），行内间距 6。

### 3.3 聊天区

外边距 12/12/12/12，纵向间距 8：

1. 标题 `Flare 对话`（16 / SemiBold / `orange`）
2. **输出区**（弹性占满 `stretch=1`）：底 `bgSoft`、边 `borderSoft` 1px、圆角 8、内边距 8、字号 14
   - 只读、可选择文本、滚动到底时自动跟随（流式期间每帧 `ensureCursorVisible`）
3. 输入行（水平，间距 8）：输入框（弹性 `stretch=1`，底 `#ffffff`，边 `borderSoft`，圆角 8，
   内边距 `8 × 10`，focus 边 `orange`，`placeholder`「输入消息…」）｜`发送`（主按钮，内边距 `8 × 18`）｜`⏹ 停止`（危险样式）

### 3.4 弹窗（记忆 / 技能 / 设置 / 版本）

四个面板在 Qt 侧都是**独立窗口**（不是内嵌页）。模态性逐项不同，**必须照抄**：

| 面板 | 标题 | 模态 | 尺寸 | 面板外边距 / 间距 |
|---|---|---|---|---|
| 记忆 | `记忆` | **非模态** | 420 × 480 | 8/8/8/8 · 8 |
| 技能 | `技能与工具` | **非模态** | 420 × 480 | 12/12/12/12 · 8 |
| 设置 | `设置` | **模态** | 440 × 320 | 20/20/20/20 · 14（表单单内间距 10） |
| 版本与更新 | `版本与更新` | **非模态** | 500 × 340 | 见下方 GroupBox |

面板内容：

- 记忆：标题 15/600；记忆列表弹性（底 `bgSoft` 边 `borderSoft` 圆角 8，字号 13，项 `padding 8×6` 圆角 6）；
  底部 `＋ 新增记忆` 主按钮 13px / 圆角 6 / 内边距 `6 × 12`
- 技能：标题 15/600；技能列表同上结构（字号 13）；底部主按钮 13px / 内边距 `6 × 12`
- 设置：标题 18/600；说明行 13px `textSoft`（`#8a6a3b`）；两个输入框（API Key / 模型，
  底 `bgSoft` 边 `borderSoft` 圆角 6 内边距 8 字号 14，focus 边 `orange`）；
  保存按钮 14px/600、圆角 8、内边距 `10 × 24`；底部状态行 12px `textMain`；
| 版本 | 见下 | GroupBox 圆角 8，边框 `borderSoft2`，`margin-top 14`，内边距 `14/14/10/14`，底 `bgSoft`；GroupBox 标题 12px 左偏移 12 内边距 `0 4` 色 `orange` 加粗 |

版本面板结构：两个 GroupBox（`Flare 客户端` / `Flare 内核`），每个含版本号（15/Bold/`textMain`）、
仓库/来源副行（12/`textMuted`）与状态副行（12/`textMuted`）、检查按钮（主按钮 13px，圆角 6，
内边距 `6 × 16`，`min-width 88`），底部一行 11px `textFaint` 说明更新来源。
**两个检查按钮 id 分别为 `guiCheckButton` / `kernelCheckButton`**，样式必须各写一份而不是共用选择器
（Qt 侧如此，避免一处改动波及另一个）。

---

## 4. 聊天流渲染规则

按引擎事件顺序渲染，**未知 `type` 静默忽略**（向前兼容硬约束）。

| 事件 | 渲染 |
|---|---|
| 欢迎态（无会话 / 新会话） | 欢迎词三行：`F L A R E`（**22px / 700**，逐字符火焰渐变，字间有空格）+ `Let your inspiration flare 🔥`（15px，顶部间距 8）+ `开始你的灵感之旅`（12px，`#b45309`，顶部间距 12）；起动呼吸动画（见 §5） |
| 用户发送 | 立即本地回显：`<b>你</b>：<内容>`，作者名 `orange` 加粗，正文 `textMain`；随后清空输入框 |
| `text`（流式增量帧，引擎即推 `{"type":"text","content":…}`） | **追加到当前「Flare」块**，不新建块；首个 `text` 前先插入块并写作者名 `Flare：`（`orange` 加粗）；逐帧滚到底 |
| `done` | 结束本次流式，恢复发送按钮可用；下一次 `text` 才开新块 |
| `cancelled` | 插入一条 `⏹ Flare：已停止生成` |
| `cancel` 请求已发出 | 立刻插入 `⏹ 已请求停止：等待引擎取消当前回复…`（本地乐观提示，等 `cancelled` 再补终态） |
| `error` | 插入 `⚠️ Flare：<message 原文>`；`message` 是中文面向用户的，**不要再翻译或改写** |
| `tool_call` | 工具卡片，图标 `🔧`，标题 `调用工具`，正文为工具名 |
| `tool_result` | 工具卡片，图标 `📦`，标题为工具名（空则 `工具结果`），正文为结果文本 |
| `confirm` | 弹出确认 UI（见 §4.1），**串行**：一次只弹一个，后续 `confirm` 入队 |
| 其它（`usage`/`models`/`version`/`memories`/…） | 不进聊天流；进状态栏或对应面板 |

### 4.1 工具卡片样式

底 `#fff3d6`、边 `borderSoft` 1px、圆角 8、内边距 `6 × 10`；
首行 `<图标> <标题>` 加粗 `orange`；次行为正文，`textSoft` + **等宽字族**；
正文可能很长 → 卡片内换行、限高（建议 180）后可展开，但**不得截断到不加提示**。

### 4.2 确认弹窗（安全相关，逐条照做）

1. 标题「确认操作」，模态，阻塞该会话的其它确认请求。
2. 正文三行（缺项省略）：
   - `AI 想调用工具「<name>」`（name 空则「未知」）
   - `说明：<description>`（引擎 v0.6.27+ 提供）
   - `参数：<key1, key2, …>` — **只列键名，绝不显示参数值**（可能含敏感内容）。
3. 按钮：「允许」（默认按钮，主按钮样式）｜「拒绝」（危险/次级样式）。
4. 回传**必须是** `{"type":"confirm_response","id":"<confirm.id>","decision":"allow_once"|"deny",…}`。
   - `允许` → `allow_once`（不是 `allow_session`，也不是 `always`）
   - `拒绝` → `deny`
   - 若未来加「本会话内总是允许」→ `allow_session`；跨会话永久 → `always`。
   - **注意字段名**：应答里的 id 对应 `confirm` 事件的 `id`；而 `confirm_result` 事件里同一个值叫 `confirmId`。
5. 超时：引擎 30s 未收到应答即按拒绝处理并把 `tool_result{isErr:true}` 喂回模型。
   宿主 UI **不要自己再设更短的超时**，也不要假装用户已答。
6. 用户取消/关闭弹窗 = 必须回 `deny`，**不能只关窗不发应答**（否则白等 30s）。

### 4.3 按钮可用性状态机

| 状态 | 发送 | 停止 | 输入框 |
|---|---|---|---|
| 空闲 | 可用 | 禁用 | 可用，回车发送 |
| 流式中（`chat` 已发未 `done`） | 禁用 | **可用** | 可用（允许下一轮排队，或保持禁用——三端必须一致，取「可用但发送禁用」） |
| 确认弹窗打开 | 禁用 | 禁用 | 禁用 |
| 引擎未连接 | 禁用 | 禁用 | 禁用，placeholder 提示引擎失败 |

---

## 5. 动画规范

**Qt 现行实现没有任何补间动画**（源码内 `QPropertyAnimation` 零命中），只有一个呼吸计时器：

| 动画 | 参数 | 三端要求 |
|---|---|---|
| 欢迎词呼吸 | 定时器 **80ms**（≈12fps，与 CLI 版一致）；相位 `sin(t*2π/5)*0.12` 叠加到渐变偏移上；`t` 为启动后秒数 | 必须实现，周期 5s，幅度 0.12 |
| 弹窗出现 | 0.92 → 1.0，spring，**350ms** | **新增（原生壳统一补）**：Windows `ScaleTransform` + `QuadraticEase`；macOS `withAnimation(.spring(response:0.35,dampingFraction:0.78))` |
| 菜单/浮动面板出现 | 上移 2px + 淡入，**150ms** | 新增 |
| 标签/提示淡入 | 不透明度 0→1，**200ms** | 新增 |
| 欢迎页首次出现 | 淡入 + 轻微上浮，**400ms** | 新增 |
| 按钮悬停/按下 | 仅颜色切换，**无位移、无缩放** | 保持 |

原则：**颜色即时切换**，位移/缩放只用于弹窗与面板进出；不做弹跳、不做旋转、不加音效。
动画时长一律读上表常量，不散落在代码里。

---

## 6. 平台落地映射

| 概念 | Qt（Linux） | WPF（Windows） | SwiftUI（macOS） |
|---|---|---|---|
| 窗口底 | `QPalette::Window` `#ffffff` | `Window.Background` | `NSColor.windowBackgroundColor` 覆盖为色值 |
| 1px 描边 | QSS `border:1px solid` | `Border.BorderThickness=1` | `overlay(RoundedRectangle().stroke(lineWidth:1))` |
| 圆角 | `border-radius` | `CornerRadius` | `.cornerRadius(_:style:.continuous)` |
| 选中项 | `QListWidget::item:selected` | `ListBoxItem.IsSelected` + Trigger | `List(selection:)` + `.listRowBackground` |
| 等宽正文 | `font-family:monospace` | `FontFamily="Consolas"` | `.monospaced()` |
| 菜单栏置空 | `setMenuBar(nullptr)` | 用无 `Menu` 的 `Window` | `NSApp.mainMenu = nil` |
| 子进程 | `QProcess` | `Process` + `RedirectStandardInput/Output` | `Process`（`Pipe`） |
| 逐行读 stdout | `readyRead` + 按 `\n` 切 | `OutputDataReceived`（**注意 `\n` 与 `\r\n` 都要切**） | `FileHandle.readabilityHandler` 缓冲切行 |
| 无边框弹窗缩放 | （无） | `RenderTransform` | `scaleEffect` |
| 高 DPI | Qt 自动 | 声明 PerMonitorV2 | Retina 自动，用 pt |

### 6.1 跨平台易错点（实测踩过）

1. **WPF 读 stdout 必须按 `\n` 切**：`OutputDataReceived` 会去掉行尾，但若引擎输出出现空行或
   一行被拆分，需自行缓冲拼接；解析失败该行要**丢弃而不是崩溃**。
2. **写入 stdin 必须带 `\n` 并 flush**，编码 UTF-8 无 BOM。
3. **`terminal` 工具输出可能含 ANSI 与超长行**，UI 侧一律按纯文本显示，不解析 ANSI。
4. **不要在 UI 线程阻塞读子进程**；三类壳子都已有异步读泵。
5. **确认弹窗必须在 UI 线程弹**，引擎事件在后台线程到达 → 需 marshal 到主线程（Qt 用信号槽、
   WPF 用 `Dispatcher.Invoke`、SwiftUI 用 `@MainActor`）。

---

## 7. 验收清单（三端逐条打勾）

**静态**
- [ ] 无菜单栏（含 macOS 顶部全局菜单栏）
- [ ] 窗口默认 980×640、最小 820×600、侧栏固定 220 不可折叠
- [ ] 全部色值与 §1 表逐项一致（抓屏取色器核对 ≥8 个点位：窗口底、输出区底、主按钮、主按钮悬停、
      列表选中、边框、标题字、正文）
- [ ] 字号阶梯按 §2（标题 15/16/18 加粗，正文 13/14，脚注 11/12）
- [ ] 输入框 focus 边框变 `#f97316`
- [ ] 危险按钮（停止）为白底红字红边，不是红底

**动态**
- [ ] 欢迎词逐字符渐变 + 5s 呼吸（肉眼可见轻微流动，不刺眼）
- [ ] 流式输出逐帧追加到同一块，不重复插入作者名
- [ ] 工具卡片浅琥珀底 + 等宽正文
- [ ] 确认弹窗只列参数键名；允许 → `allow_once`、拒绝 → `deny`；关窗也会回 `deny`
- [ ] 点停止后先出本地乐观提示，收到 `cancelled` 后补 `⏹ 已停止生成`
- [ ] 引擎未安装时状态栏提示「引擎启动失败」，输入禁用，不崩溃
- [ ] 未知事件 `type` 不弹错、不崩

**一致性**
- [ ] 三端对同一份 `testdata/protocol/stream-*.jsonl` 回放，界面**元素数量与文案逐字相同**
      （可用「回放器」模式：把语料灌进 UI，比对文本快照）

---

## 8. 变更流程

1. 改本文件（数值/语义）→ 2. 改 Qt 实现 → 3. 抓屏核对 → 4. 同步 WPF/SwiftUI → 5. 三端清单打勾。
只改代码不改本文件的样式改动，评审**直接打回**。

---

## 附录 取证索引（数值 → 源码位置）

本文件每个数值都能在 `flare-client/src/` 找到出处，改规范时按表复核：

| 内容 | 位置 |
|---|---|
| 色阶 / 米白底 / 文字色 / `flameColor` / `breathColor` / `gradientHtml` / 欢迎词文案 | `flametheme.h`（全文） |
| 默认窗口 `resize(1024,700)` | `main.cpp:30` |
| `setWindowTitle("Flare")` / `resize(980,640)` / `setMinimumSize(820,600)` / 无菜单栏 | `mainwindow.cpp:31-32` |
| 拆分器 `setStretchFactor(1,1)` / `setCollapsible(0,false)` | `mainwindow.cpp:71` |
| 记忆 / 技能 / 设置对话框尺寸与模态性 | `mainwindow.cpp:165` / `:180` / `:195` |
| 状态栏样式（12px / `#b45309` / 链接悬停） | `mainwindow.cpp:106` |
| 聊天区标题 16px·600 / 输出区 14px / 输入框 / 发送按钮 / 停止按钮 | `chatwidget.cpp:29` / `:37` / `:45` / `:50` / `:58` |
| 呼吸定时器 80ms / 呼吸相位 `sin(t*2π/5)*0.12` | `chatwidget.cpp:71-73` / `:322` |
| 工具卡片 HTML（`#fff3d6` 底 / `borderSoft` 边 / 圆角 8 / 等宽正文） | `chatwidget.cpp:217` |
| 确认弹窗（只列参数键名 / `allow_once` / `deny`） | `chatwidget.cpp:236` |
| 欢迎词 22px·700 与副行 15px / 尾行 12px `#b45309` | `chatwidget.cpp:338` / `:365-366` |
| 侧栏固定 220 / 搜索框 / 列表项 / 三行按钮 | `sessionlistwidget.cpp:9` / `:49` / `:56` / `:67+` |
| 记忆面板 15px 标题 / 13px 列表 / 13px 按钮 | `memorypanel.cpp:18` / `:23` / `:31` |
| 技能面板（同上，外边距 12） | `skillpanel.cpp:23` / `:28` / `:36` |
| 设置面板 18px 标题 / 说明 13px / 输入 14px / 保存按钮 / 状态 12px | `settingspanel.cpp:32` / `:41` / `:50` / `:66` / `:70` |
| 版本面板 `resize(500,340)` 非模态 / GroupBox 样式 / 副行 12px / 尾注 11px / `min-width:88` | `versionpanel.cpp:46` / `:50-58` / `:75` / `:136` |
