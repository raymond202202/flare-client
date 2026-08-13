# flare-client 夜间自动迭代 · 进度

> 启动：2026-08-12 下午（用户授权自动迭代至晚上验收）
> 项目：~/hermes-projects/flare-client（Qt 6 Widgets 客户端）
> 协作文档：https://eqmdtenvlk7.feishu.cn/docx/AAmedhKyrodsx4xQT7ccqkfynzj

## 【✅ 已完成】M3 完整功能迭代（2026-08-12 全部交付）

- [x] M3-1 会话列表侧边栏：list_sessions 加载 + 新建/切换/删除会话（create_session/delete_session 协议）
- [x] M3-2 记忆查看面板：get_memories 加载 + 展示（协议已支持）
- [x] M3-3 流式渲染优化：text 事件增量追加（当前是整段 append，改为 chunk 级增量显示）
- [x] M3-4 工具调用可视化：tool_call/tool_result 卡片式展示（🔧工具名 + 📦结果）
- [x] M3-5 确认门：confirm 事件弹窗（允许/拒绝按钮）+ confirm_result 回传（协议已支持）
- [x] M3-6 模型信息：启动时 models 请求展示当前模型/provider（仅显示，不泄露 key）
- [x] M3-7 全量测试回归：ctest 全绿 + e2e 冒烟 PASS + 截图留档
- [x] 闪退修复（2026-08-12 晚）：引擎事件重入崩溃 + confirm 弹窗嵌套事件循环（commit 40db764）

## 【✅ 已完成】M4 打包交付 + 体验打磨（2026-08-13 全部交付）

- [x] M4-1 Fedora RPM 打包：CMake CPack → .rpm，安装到系统验证开始菜单可启动
- [x] M4-2 Windows 打包流程：交叉编译/CI 方案（Qt 6 静态或 mingw），产出 exe 安装包
- [x] M4-3 macOS dmg：macmini 端已产出（37M），fedora 侧提供 CMake 支持验证
- [x] M4-4 自动清理联动：打包后自动执行 cleanup-old-builds.sh（保留最新 3 个）
- [x] M4-5 体验打磨：按用户实机反馈修复（闪退残余问题、UI 细节、易用性）

## 【🔴 当前最高优先级】M5 用户反馈改版（2026-08-13 用户实机意见）

> 用户意见原文：「我还没看产品…主色调改为之前做命令行版时我要求的红橙黄等火焰活力色系。每个会话都要在会话之初展示我们之前命令行版设计的跃动版欢迎词。左边面板我看到了记忆按钮，但没看到skill按钮，左边面板每个会话展示的太简单，让人看不出来哪个会话是什么，且点进去看不到历史消息。」
> 视觉规范来源：`~/hermes-projects/flare/src/cli/flame-banner.ts`（FLAME_TOKENS：红 #ef4444 / 橙 #f97316 / 琥珀 #f59e0b / 黄 #fbbf24；flameColor() 红→橙→黄插值；欢迎词 "F L A R E" + "Let your inspiration flare 🔥"）

- [x] M5-1 火焰主题换肤：全 UI 紫色 → 火焰色系（主色橙 #f97316、强调红 #ef4444、高亮黄 #fbbf24），移植 flameColor() 渐变到 Qt（QColor 插值）
- [x] M5-2 跃动欢迎词：每个会话开始时展示命令行版同款欢迎词（F L A R E + Let your inspiration flare 🔥，火焰渐变 + 呼吸动画 QTimer 驱动）
- [x] M5-3 Skill 按钮：左侧面板新增 Skill 入口（发 `tools` 命令 → 展示可用工具/技能列表面板）
- [x] M5-4 会话列表增强：改用 `recent_sessions`（含 preview 首条消息预览 + updatedAt 时间）展示，让会话可识别
- [x] M5-5 历史消息加载：切换会话时发 `get_messages` 加载历史消息并渲染到消息区（当前切会话只清空）
- [x] （2026-08-13 02:10 第 15 轮核验）M4 全部完成待用户验收；飞书文档已补同步 M4 条目（revision 35）

## 【✅ 已完成】M6 用户新增需求（2026-08-13 交付）

- [x] M6-1 图标改火焰色：packaging/flare-client.svg 紫色火焰 → 红橙黄渐变火焰；修复安装版从未加载自定义图标 bug（appDir 相对路径）；main.cpp 新增 --screenshot 自截图参数（物理屏黑屏时验证 UI）
- [x] M6-2 设置入口：⚙️ 设置按钮 → 弹窗填写 API key（DEEPSEEK_API_KEY + DEFAULT_MODEL），读写 ~/.flare/.env（chmod 600、保留其他变量、Password 掩码）；新增 settingspanel 测试 → 7/7 全绿
- [x] M6-3 CLI/GUI 配置共享验证：~/.flare/.env（key）+ flare.db（记忆/会话）天然共享（DB 实测 cli-share-test 会话可见）；CLI flare v0.6.110 与 GUI 同引擎同库
- [x] M6-4 体验打磨核验（自动迭代轮完成，commit f09e051）：修复 6 处新 UI 问题——
  ① 串会话防护：引擎所有会话事件带 sessionId，ChatWidget 校验归属，旧会话流式回复不再串入新会话（含测试）
  ② 切换会话时旧会话流式先发 cancel 再 getMessages（含测试）
  ③ cancelled 事件处理：停止生成生效关闭流 + 收起提示（含测试）
  ④ 空会话展示跃动欢迎词替代「暂无消息」（含测试）
  ⑤ openSettingsPanel 补 WA_DeleteOnClose 修复对话框泄漏
  ⑥ SettingsPanel model 留空不再写空值行；CMake POST_BUILD 复制 flare-icon.svg 到可执行目录（修复便携版图标未生效遗留）
  质量：构建 0 错误 + 7/7 ctest 全绿（chatwidget 20 子测试）+ e2e 真实对话 PASS + 截图像素验证（火焰主题配色正确）

## 迭代记录

| 轮次 | 时间 | 完成 | 构建 | 备注 |
|------|------|------|------|------|
| (基线) | 2026-08-12 13:05 | M0-M2 | ✅ 4/4 测试 | 手动完成 |
| 1 | 2026-08-12 深夜 | M3-1 | ✅ 5/5 测试 + e2e PASS | SessionListWidget 侧边栏；EngineBridge 增 create/delete_session；真实引擎 create→list→delete 往返测试 |
| 2 | 2026-08-12 深夜 | M3-2 | ✅ 6/6 测试 + e2e PASS | MemoryPanel 记忆查看；EngineBridge 增 get_memories；侧边栏「🧠 记忆」入口 → 弹窗；真实 get_memories 往返 |
| 3 | 2026-08-12 深夜 | M3-3 | ✅ 6/6 测试 + e2e PASS | 流式渲染：text chunk 增量追加同块（QTextCursor + 紫色标签）；done/tool_call/error/用户新消息自动闭合流；4 个新流式测试 |
| 4 | 2026-08-12 深夜 | M3-4 | ✅ 6/6 测试 + e2e PASS | 工具卡片：tool_call → 🔧调用工具+工具名；tool_result → 📦工具名+结果；浅紫底圆角卡片 insertHtml；2 个新卡片测试 |
| 5 | 2026-08-12 深夜 | M3-5 | ✅ 6/6 测试 + e2e PASS | 确认门：confirm 事件 QMessageBox 弹窗（允许/拒绝，紫配）；respondConfirm → confirm_result(allow_once/deny)；EngineBridge 增 confirmResult + requestSent 信号；弹窗只展示参数键名不泄露值 |
| 6 | 2026-08-12 深夜 | M3-6 | ✅ 6/6 测试 + e2e PASS + models 探针 | 模型信息：启动 queryModels → 状态栏「🤖 model · provider」；只读 model/provider 字段，绝不展示 apiKey/baseURL（测试含泄密防护断言） |
| 7 | 2026-08-12 深夜 | M3-7 | ✅ 6/6 测试 + e2e PASS + 截图 | 全量回归：ctest 6/6 全绿；e2e 真实对话 PASS；gui_capture 截图留档 docs/flare-m3-preview.png（流式块/工具卡片/模型信息演示） |
| 8 | 2026-08-12 深夜 | 文档同步修复（无代码变更） | ✅ 6/6 测试（复验 HEAD） | M3 全阶段已完成无未完成项；macmini 已跟进至 v0.4.0-M3-7（编译 0 错 + 6/6 绿 + dmg 37M）无待处理反馈；修复飞书文档结构：M3 迭代条目（M3/M3-2/M3-1）归位第五章标记区（最新在最上）、删除文档末尾/第九章重复条目、第六章追加 2 条 fedora 回应（协作断链①②已修复 + 清理策略采纳）；git 无新提交（代码未变，HEAD=4c07b4d） |
| 9 | 2026-08-12 深夜 | 夜间核验（无代码变更） | ✅ 6/6 测试 + 构建 0 错误 | 读 macmini 日志：最新 2 条为 v0.4.0-M3-4 / v0.4.0-M3-7 跟进确认（编译 0 错 + 6/6 绿 + dmg），无新问题/建议/修复请求（协作断链+清理策略反馈已有回应）；progress.md 无未完成 [ ] 阶段（M3-1~M3-7 全勾选）→ 按铁律不再自循环；复验构建 0 错误 + ctest 6/6 全绿；git 无新提交（HEAD=c1883de），等待用户验收 |
| 10 | 2026-08-12 深夜 | 夜间核验（无代码变更） | ✅ 6/6 测试 + 构建 0 错误 | 读 macmini 日志：最新条目仍为 v0.4.0-M3-7（自动跟进）/ v0.4.0-M3-4（手动跟进）确认（编译 0 错 + 6/6 绿 + dmg 37M/41M），无新问题/建议/修复请求；文档内「建议」6 处均为第三章选型调研历史内容，非新反馈；progress.md 无未完成 [ ] 阶段（M3-1~M3-7 全勾选）→ 按铁律不再自循环；复验构建 0 错误 + ctest 6/6 全绿（1.78s）；git 工作区干净（HEAD=6e4d98c），无代码变更，等待用户验收 |
| 11 | 2026-08-12 深夜 | 夜间核验（无代码变更） | ✅ 6/6 测试 + 构建 0 错误 | 读 macmini 日志：最新条目仍为 v0.4.0-M3-7 自动跟进确认（编译 0 错 + 6/6 绿 + dmg 37M），无新问题/建议/修复请求（协作断链+清理策略回应均已在日志结束标记内）；progress.md 无未完成 [ ] 阶段（M3-1~M3-7 全勾选）→ 按铁律不再自循环；复验构建 0 错误 + ctest 6/6 全绿（1.79s）；git 工作区干净（HEAD=51d19df），无代码变更，等待用户验收 |
| 12 | 2026-08-12 深夜 | 夜间核验（无代码变更） | ✅ 6/6 测试 + 构建 0 错误 | 读 macmini 日志：最新条目仍为 v0.4.0-M3-7 自动跟进确认（编译 0 错 + 6/6 绿 + dmg 37M），无新问题/建议/修复请求（协作断链+清理策略回应均在日志结束标记内，无需追加回应）；progress.md 无未完成 [ ] 阶段（M3-1~M3-7 全勾选）→ 按铁律不再自循环；复验构建 0 错误 + ctest 6/6 全绿（1.79s）；git 工作区干净（HEAD=b592758），无代码变更，等待用户验收 |
| 13 | 2026-08-13 00:25 | M4-1 + M4-3 + M4-2（部分） | ✅ 6/6 测试 + 构建 0 错误 + RPM 产出 | M4-1 Fedora RPM 打包：CMakeLists 加 CPack RPM（修复 include(CPack) 位置坑——必须在 CPACK_* 变量后）、install 规则（bin + desktop + SVG 图标）、版本 0.4.0；build-fedora.sh -p 真打 RPM → packaging/dist/ + cleanup 联动；rpm2cpio 解包验证 + offscreen 冒烟 8s 存活 PASS（commit 45af59a）。M4-3 macOS dmg CMake 支持：Darwin 分支（DragNDrop + /Applications + 卷名）已随 M4-1 提交，模拟 Darwin 交叉配置验证 CPackConfig 生效（DMG_VOLUME_NAME=Flare Client 0.4.0）。M4-2 Windows：本机无 mingw/wine 且无 sudo 装包 → 落地 GitHub Actions CI（windows-latest + aqtinstall Qt 6.7.3 MSVC + windeployqt + CPack ZIP）+ build-windows.ps1 增强（-Package 参数 windeployqt+CPack+清理）；模拟 Windows 交叉配置仅工具链链接失败、CMake 语法正常 |
| 14 | 2026-08-13 00:33 | M4-4 + M4-5 | ✅ 6/6 测试 + 构建 0 错误 + e2e PASS + RPM 产出 | M4-4 自动清理联动：build-fedora.sh -p 实跑完整链路（构建→测试→CPack→收集→清理）；cleanup 删除逻辑实测（5 包→保留 3 删 3）；build-macos.sh 已有联动（commit 6177549）。M4-5 体验打磨：①EngineBridge 防滞留补读（嵌套事件循环期间数据未读时 singleShot 补读，防数据永久丢失）②MainWindow setMenuBar(nullptr) 显式无菜单栏 + 窗口图标（RPM 路径/开发路径双回退）③新增「⏹ 停止」按钮（流式时显示、done/error 隐藏、发 cancel 协议）+ 3 个新测试；全部 ctest 6/6 绿（chatwidget 15 子测试）+ e2e 真实对话 PASS + RPM 重打包 50K |
| 15 | 2026-08-13 02:10 | M4 全部完成核验（无代码变更） | ✅ 6/6 测试 + 构建 0 错误 | 读 macmini 日志：最新条目仍为 v0.4.0-M3-7 自动跟进（编译 0 错 + 6/6 绿 + dmg 37M），无新问题/建议/修复请求（协作断链+清理策略回应均已在日志结束标记内）；progress.md M4-1~M4-5 已全勾选且 git 确认提交落地（45af59a M4-1/3fe1b14 M4-2/6177549 M4-4/3950a8b M4-5，M4-3 折叠于 M4-1 Darwin 分支）；复验构建 0 错误 + ctest 6/6 全绿（1.84s）；RPM 产物 packaging/dist/flare-client_0.4.0.rpm（50K）在；发现飞书文档缺 M4 条目（迭代日志最新仍为 M3-7）→ 补同步 M4 完成条目（revision 35，macmini 全文扫描可检测）；M4 全部完成 → 按铁律不再自循环，等待用户验收 |
| 16 | 2026-08-13 夜间 | M4 完成核验（无代码变更） | ✅ 6/6 测试 + 构建 0 错误 | 读 macmini 日志：最新条目仍为 v0.4.0-M3-7 自动跟进确认（编译 0 错 + 6/6 绿 + dmg 37M），无新问题/建议/修复请求（第六章仅历史 fedora 回应，无新条目）；progress.md M4-1~M4-5 全勾选无未完成阶段 → 按铁律不再自循环；复验构建 0 错误 + ctest 6/6 全绿（1.81s）；git 工作区干净（HEAD=2a7b630）；确认飞书文档末尾已含 v0.4.0-M4 条目（revision 35 生效，两条 M4 记录，macmini 全文扫描可检测）→ 无需重复同步；RPM 产物 packaging/dist/flare-client_0.4.0.rpm（50K）在；M4 全部完成，等待用户验收 |
| 17 | 2026-08-13 夜间 | M4 完成核验（无代码变更） | ✅ 6/6 测试 + 构建 0 错误 | 读 macmini 日志：最新条目仍为 v0.4.0-M3-7 自动跟进确认（编译 0 错 + 6/6 绿 + dmg 37M），无新问题/建议/修复请求；文档末尾两条 v0.4.0-M4 条目仍在（revision 35 生效，待 macmini 跟进，非新反馈）；progress.md M4-1~M4-5 全勾选无未完成阶段 → 按铁律不再自循环；复验构建 0 错误 + ctest 6/6 全绿（1.82s）；git 工作区干净（HEAD=2015d60）；RPM 产物 packaging/dist/flare-client_0.4.0.rpm（50K）在；M4 全部完成，等待用户验收 |
| 18 | 2026-08-13 夜间 | M4 完成核验（无代码变更） | ✅ 6/6 测试 + 构建 0 错误 | 读 macmini 日志：最新条目仍为 v0.4.0-M3-7 自动跟进确认（编译 0 错 + 6/6 绿 + dmg 37M），无新问题/建议/修复请求；文档末尾两条 v0.4.0-M4 条目仍在（revision 35 生效，待 macmini 跟进，非新反馈）；progress.md M4-1~M4-5 全勾选无未完成阶段 → 按铁律不再自循环；复验构建 0 错误 + ctest 6/6 全绿（1.81s）；git 工作区干净（HEAD=b940a6e）；RPM 产物 packaging/dist/flare-client_0.4.0.rpm（50K）在 + 安装版 ~/.flare-client/install/flare-client 在；M4 全部完成，等待用户验收 |
| 19 | 2026-08-13 11:58 | M6-4 体验打磨 | ✅ 7/7 测试 + 构建 0 错误 + e2e PASS + 截图 | 读飞书第六章：macmini 最新跟进仍为 v0.4.0-M4（手动，修 MACOSX_BUNDLE），无 M5/M6 新反馈；progress.md M6-1~M6-3 已勾选（主会话交付）→ 本轮做 M6-4 体验打磨核验。审查发现 4 真实 bug + 2 体验问题并全部修复（commit f09e051）：①串会话防护（引擎所有会话事件带 sessionId，ChatWidget 校验归属，旧会话流式不串入新会话，含 2 测试）②切换会话时旧会话流式先发 cancel 再 getMessages（含测试）③cancelled 事件处理：停止生成生效关闭流+收起提示（含测试）④空会话展示跃动欢迎词替代「暂无消息」（含测试）⑤openSettingsPanel 补 WA_DeleteOnClose 修复每次打开泄漏 QDialog ⑥SettingsPanel model 留空不再写 DEFAULT_MODEL= 空值行 + CMake POST_BUILD 复制 flare-icon.svg 到可执行目录（修复 M6-1 便携版图标从未真正生效的遗留）。质量：构建 0 错误 + ctest 7/7 全绿（chatwidget 20 子测试）+ e2e 真实对话 PASS（欢迎词+回复完整）+ --screenshot 截图像素验证（米白底 #fffbf0 主、火焰橙 #f97316 高亮、浅橙边框，2250 色） |
| 20 | 2026-08-13 13:47 | M7-1 增量打磨（同步 macmini 修复） | ✅ 7/7 测试 + 构建 0 错误 + Darwin 交叉配置验证 | 读飞书第六章：macmini 最新跟进为 v0.4.0-M4（2026-08-13 手动跟进，编译 0 错 + 6/6 绿 + dmg 65M），其中反馈「修复 CMakeLists Darwin 分支缺 MACOSX_BUNDLE 属性（fedora 原版只在 Linux 模拟验证，真 mac 打出的 dmg 无 .app）」→ 按流程先处理：fedora 端 CMakeLists Darwin 分支补 set_target_properties(MACOSX_BUNDLE TRUE + BUNDLE_NAME/GUI_IDENTIFIER/BUNDLE_VERSION/SHORT_VERSION)，同步回源码避免 macmini 下次同步丢修复（commit d4c7944）。验证：Linux 构建 0 错误 + ctest 7/7 全绿（2.02s）；cmake -DCMAKE_SYSTEM_NAME=Darwin 交叉配置成功（DragNDrop + DMG_VOLUME_NAME "Flare Client 0.4.0" + /Applications 前缀均生效，证明 if 分支进入且 set_target_properties 语法通过）；安装目录已更新（210936 字节 13:47）。progress.md 无未完成 [ ] 阶段 → 本轮为增量打磨，不新增阶段 |

## 构建命令（每轮必须执行）

```bash
cd ~/hermes-projects/flare-client
cmake -B build-fedora -DCMAKE_BUILD_TYPE=Release && cmake --build build-fedora -j$(nproc)
ctest --test-dir build-fedora --output-on-failure
```

## 铁律（每轮必读）

1. **禁止 git push**（任何情况）；只本地 commit
2. **构建 0 错误 + ctest 全绿才 commit**；构建失败修复最多 3 次，仍失败记录到 progress.md 并跳下一阶段
3. **不动 flare 主项目和其他仓库**（只读参考）
4. **UI 遵循用户偏好**：浅色白底紫配 #6d4aff、无菜单栏（Menu.setApplicationMenu(null) 精神：Qt 用 setMenuBar(nullptr)）、简洁轻量
5. **安全红线**：任何地方不得写入/打印 API key、密码、敏感信息；lark-cli 输出含凭据时值一律打码
6. 每个阶段完成后：`git commit` + **更新 progress.md（勾选+追加记录行）** + **更新飞书文档迭代日志**（命令见下）
7. 不要问问题，自主推进；自循环：本轮开始未超 15 分钟且还有未完成阶段 → 回到第一步继续

## 飞书文档同步（每阶段完成后执行）

```bash
cat << 'EOF' | lark-cli docs +update --api-version v2 --doc "https://eqmdtenvlk7.feishu.cn/docx/AAmedhKyrodsx4xQT7ccqkfynzj" --command append --content - --doc-format markdown
v0.4.0-M3 | 2026-08-12 | <完成内容简述> | 待 macmini 跟进
EOF
```

注意：lark-cli 追加在文档末尾（迭代日志章节）；若找不到 lark-cli 命令则记录到 progress.md 即可（飞书同步由主 agent 补）

## 冒烟验证（每轮可选，M3-7 必做）

```bash
cd ~/hermes-projects/flare-client
PATH=/usr/bin:$PATH timeout 60 env QT_QPA_PLATFORM=offscreen ./build-fedora/tests/e2e_acceptance
```
