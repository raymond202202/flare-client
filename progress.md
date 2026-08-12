# flare-client 夜间自动迭代 · 进度

> 启动：2026-08-12 下午（用户授权自动迭代至晚上验收）
> 项目：~/hermes-projects/flare-client（Qt 6 Widgets 客户端）
> 协作文档：https://eqmdtenvlk7.feishu.cn/docx/AAmedhKyrodsx4xQT7ccqkfynzj

## 【🔴 当前最高优先级】M3 完整功能迭代（按序）

- [x] M3-1 会话列表侧边栏：list_sessions 加载 + 新建/切换/删除会话（create_session/delete_session 协议）
- [x] M3-2 记忆查看面板：get_memories 加载 + 展示（协议已支持）
- [x] M3-3 流式渲染优化：text 事件增量追加（当前是整段 append，改为 chunk 级增量显示）
- [x] M3-4 工具调用可视化：tool_call/tool_result 卡片式展示（🔧工具名 + 📦结果）
- [x] M3-5 确认门：confirm 事件弹窗（允许/拒绝按钮）+ confirm_result 回传（协议已支持）
- [x] M3-6 模型信息：启动时 models 请求展示当前模型/provider（仅显示，不泄露 key）
- [x] M3-7 全量测试回归：ctest 全绿 + e2e 冒烟 PASS + 截图留档

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
