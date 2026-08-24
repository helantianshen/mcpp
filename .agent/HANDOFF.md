# 当前目标

在 mcpp 核心工具链中提供可选的“牛来”编译结果原声通知：成功随机播放两段“妈妈”，失败播放“牛来”。

# 当前状态

开发态实现与验证均已完成，开发内容已提交并推送到 `helantianshen/mcpp` fork 的 `mcpp` 分支（实现提交 `093e826`）；音频授权确认和发布包资源注入按用户要求延后。

# 已完成工作

- `mcpp build --niulai` 成功时随机播放 `mama1.mp3` / `mama2.mp3`，失败时播放 `reply.mp3`。
- `--configure-only` 不播放；未传 `--niulai` 时行为不变；工作区构建只在最外层播放一次。
- 播放器优先级：Windows MediaPlayer、macOS `afplay`、Linux `ffplay` / `mpv` / `mpg123`；WSL 可桥接 Windows MediaPlayer。
- MP3 播放或资源查找失败时回退原有系统 TTS；通知失败不改变构建退出码。
- 开发二进制从仓库 `assets/niulai/` 查找资源，并预留发布布局 `<安装根>/share/mcpp/niulai/`。
- 三份上游音频已保存并在 `assets/niulai/NOTICE.md` 记录来源、哈希和授权待确认状态。

# 重要决策

- 功能只落在核心工具链，不修改 mcpp-vscode。
- 使用显式 CLI 开关，默认关闭。
- 不引入音频库，复用系统播放器和现有进程执行能力。
- 当前不把 MP3 注入发布包；取得上游作者许可后再调整发布流程。

# 修改 / 重要文件

- `src/cli.cppm`
- `src/cli/cmd_build.cppm`
- `src/platform/niulai.cppm`
- `tests/e2e/267_niulai_build_notification.sh`
- `assets/niulai/{mama1.mp3,mama2.mp3,reply.mp3,NOTICE.md}`

# 验证情况

- 通过：使用 mcpp 2026.8.11.2 引导器执行 `mcpp build --cache local`。
- 通过：新二进制帮助显示 `--niulai` 的原声行为。
- 通过：定向 E2E 覆盖成功随机妈妈文件、失败 reply 文件、`--configure-only` 静默。
- 通过：新二进制完整执行 `mcpp test`，92 passed、0 failed。
- 通过：`bash -n tests/e2e/267_niulai_build_notification.sh`。
- 通过：本机 WSL 实际播放成功音频；空项目实际播放失败音频并保留退出码 2；Windows 临时 MP3 已清理。

# 已知问题 / 风险

- 上游 README 将牛来声音描述为电影“原声（已降噪）”。发布前必须向上游作者 whitefirer（公开昵称 MortyWang）确认再分发许可。
- 当前发布工作流尚未把 MP3 放入 `<安装根>/share/mcpp/niulai/`，因此只有源码/开发构建可直接找到原声；缺少资源时会安全回退 TTS。
- 已在 WSL + Windows MediaPlayer 实听；原生 Windows、macOS 和普通 Linux 播放器仍需对应平台验证。
- 当前 PATH 中的 mcpp shim 指向不存在的 2026.8.17.1；本次直接使用已安装的 2026.8.11.2 引导器，未修改用户全局配置。
- `origin` 保持指向 `mcpp-community/mcpp`；`fork` 指向 `helantianshen/mcpp`，本地 `mcpp` 跟踪 `fork/mcpp`。

# 剩余工作

发布前需要取得音频许可、将三份 MP3 注入各平台发布包，并补发布包内资源存在性冒烟测试。

# 推荐下一步

审阅 `https://github.com/helantianshen/mcpp/tree/mcpp`；不要在未获单独授权时创建 PR。准备发布时联系 https://github.com/whitefirer，然后完善 release 打包与各平台实听。
