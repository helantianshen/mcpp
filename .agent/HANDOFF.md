# 当前目标

让牛来版 mcpp 在 Windows GNU 路径通过构建，并由 Windows 原生 Codex 复验编译、链接和音频行为。

# 当前状态

- Windows 原生 MSVC 构建与牛来实听已经通过。
- 原生 MinGW GCC 16.1.0 日志中的 `conflicting language linkage` 已按实证方案修复，等待原生复验。
- WSL 的 Linux → Windows GNU 全量交叉构建已证明原失败模块全部编译成功，但链接阶段暴露新的多重定义问题；因此 GNU 发布基线仍未通过。
- 修复及本交接文档位于 `helantianshen/mcpp` 的 `mcpp` 分支；Windows Codex 应测试该分支最新 HEAD。

# 已实施修复

以下三个模块都在全局模块片段的 `windows.h` 前预包含 `<cstdlib>`：

- `src/platform/windows/bounded_process.cppm`
- `src/platform/scaffold_fs.cppm`
- `src/build/schedule/detach_codegen.cppm`

MinGW 的 `winnt.h` 会在 `extern "C"` 中经 `x86intrin.h` 间接进入 libstdc++ 的 `cstdlib`。预先以正常 C++ linkage 包含 `<cstdlib>`，可避免随后 `import std` 时产生 linkage 冲突。每一行都带有顺序约束注释，不应作为“未使用 include”删除。

# 已有验证

- 原始 Windows 日志：`.agent/logs/windows-gnu-build-error-da15ced.log`。
- 同一 Windows 原生 GCC 16.1.0 最小探针：原写法退出码 1；`windows.h` 前加入 `<cstdlib>` 后退出码 0。
- WSL 执行：

  ```text
  mcpp build --target x86_64-windows-gnu --cache local
  ```

  三个原失败目标的对象和 GCM 均已生成，原 linkage 错误未再出现：

  ```text
  obj/mcpp/src/platform/windows/bounded_process.m.o
  obj/scaffold_fs.m.o
  obj/detach_codegen.m.o
  gcm.cache/mcpp.platform.windows.bounded_process.gcm
  gcm.cache/mcpp.platform.scaffold_fs.gcm
  gcm.cache/mcpp.build.schedule.detach_codegen.gcm
  ```

- 该交叉构建最终在链接 `bin/mcpp.exe` 时失败，第一类错误为：

  ```text
  multiple definition of `std::_Sp_make_shared_tag::_S_ti()::__tag'
  multiple definition of nlohmann serializer/dtoa static data
  ```

  这与本次三处 `windows.h` include 顺序不同，是后续独立阻塞。原生 MinGW 是否同样出现，必须由本轮 Windows 复验确认。

# Windows Codex 复验步骤

必须使用原生 Windows PowerShell，不得用 WSL 结果代替。请在全新工作树或全新的 `target/x86_64-windows-gnu` 中测试，避免复用 `da15ced` 的失败 BMI。

## 1. 构建并保存日志

```powershell
$env:MCPP_LOG_LEVEL = 'debug'
mcpp build --target x86_64-windows-gnu 2>&1 |
    Tee-Object windows-gnu-build-after-cstdlib.log
$BuildExit = $LASTEXITCODE
"build exit: $BuildExit"
```

先检查日志中是否还存在：

```text
conflicting language linkage for imported declaration
```

如果仍存在，报告第一处完整 include chain。如果已消失但出现 `multiple definition`，保存完整链接日志并停止扩大修改；这表示本轮 include 修复有效，但 GNU 分支还有第二个发布阻塞。

## 2. 只有链接成功后才实听

找到本轮新生成的 `mcpp.exe`，复制到源码树外，再执行：

- 正常 C/C++ 工程：`mcpp.exe build --target x86_64-windows-gnu --niulai`，退出码应为 0，并听到 `mama1.mp3` 或 `mama2.mp3`。
- 制造编译错误后执行同一命令，退出码应非 0，并听到 `reply.mp3`。
- 不传 `--niulai` 时应静默。
- `--niulai --configure-only` 应静默。
- 命令结束后 `%TEMP%` 不应新增 `mcpp-niulai-*-*.mp3`。

# 结果回传模板

```text
测试分支与 HEAD：
Windows / PowerShell：
bootstrap mcpp：
MinGW GCC：
完整构建退出码：
原 conflicting language linkage：已消失 / 仍存在
是否出现 multiple definition：
新 mcpp.exe 路径：
成功音频：
失败音频：
静默场景：
临时 MP3：
日志路径：
```

# 次要观察与风险

- bootstrap mcpp 2026.8.11.3 不认识当前 manifest 的 `bmi_schedule`，该 warning 不是原 linkage 失败原因。
- `build.mcpp running` 后曾出现 `The system cannot find the path specified.`，但牛来生成模块和对象已成功产出；构建主阻塞解决后若仍出现再单独调查。
- 发布前仍需取得 whitefirer（MortyWang）的音频再分发许可。

# 推荐下一步

Windows Codex 先完成原生 GNU 复验并回传新日志。若 linkage 错误消失但链接多重定义复现，再把 ODR 链接问题作为单独根因处理。
