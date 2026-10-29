Flare · Windows 版
==================

Flare 是一个本地优先的 AI 助手客户端。这个包里已经带好了引擎，
你不需要安装 Node、Python 或任何运行时，也不需要关心版本号。


一、运行
--------

解压后双击 Flare.exe 即可。

首次启动它会：
  · 在  %USERPROFILE%\.flare\  下建好数据目录；
  · 如果还没有配置，会在界面里引导你填 API Key。

如果你的 Windows 弹出「已保护你的电脑」（SmartScreen），
点「更多信息」→「仍要运行」即可。这是因为安装包还没买代码签名证书，
不是病毒。


二、目录结构
------------

  Flare.exe            图形界面
  Flare.exe.config     运行时配置（不要删）
  Newtonsoft.Json.dll  依赖库（不要删）
  engine\flare.exe     内置引擎（不要删）

这四个文件必须放在同一个文件夹里。把这个文件夹整体搬到别处也能用，
放到 D 盘、U 盘都行，不会写注册表。


三、命令行怎么用
----------------

如果你想要纯命令行版本（没有图形界面），去下载 engine 包，
里面只有 flare.exe 一个文件，可以直接：

  flare.exe              进入交互式对话
  flare.exe --help       看所有参数
  flare.exe --version    看版本号


四、数据在哪
------------

  %USERPROFILE%\.flare\            配置与会话记录
  %USERPROFILE%\.flare\.env        你的 API Key（别外传）

卸载就是删掉解压出来的文件夹和上面这个目录，没有残留。


五、出问题了
------------

把它从一个终端里启动，可以看到日志：

  cd 到 Flare.exe 所在目录
  .\Flare.exe

报错信息复制给我们即可。
