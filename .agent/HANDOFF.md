# 当前目标

在原生 Windows（不是 WSL）构建并实测牛来版 mcpp，确认嵌入式 MP3、PowerShell 播放、构建结果映射和临时文件清理均正常。

# 测试基线

- 仓库：`https://github.com/helantianshen/mcpp.git`
- 分支：`mcpp`
- 待测实现提交：`da15ced`
- 必须在 Windows PowerShell 中运行；WSL → Windows 播放已经验证，不算本轮结果。
- 最低基线为原生 Windows 上的 `x86_64-windows-gnu`；如果已安装 Visual Studio C++ 工作负载，再补测仓库默认的 LLVM/MSVC ABI 构建。

# 已完成实现

- `build.mcpp` 将 `mama1.mp3`、`mama2.mp3`、`reply.mp3` 生成为 C++ 模块并链接进 mcpp，发布二进制不再依赖外置音频目录。
- `mcpp build --niulai` 成功时随机播放 `mama1.mp3` / `mama2.mp3`，失败时播放 `reply.mp3`。
- Windows 优先调用系统 `powershell.exe` 的 `System.Windows.Media.MediaPlayer`；失败后才尝试 `ffplay`，再失败则使用系统语音。
- 音频写入随机临时文件，播放结束后自动删除；播放器执行上限为 5 秒。
- `--niulai` 不改变原构建退出码，`--configure-only` 不播放。

# Windows Codex 执行步骤

## 1. 记录原生环境

```powershell
Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, OSArchitecture
$PSVersionTable.PSVersion
Get-Command powershell.exe
mcpp --version
mcpp self env
```

如果尚未安装用于自托管的发布版 mcpp，按仓库 README 的 Windows 安装方式执行；不要为播放功能额外安装 FFmpeg。

## 2. 构建 Windows 原生产物

在仓库根目录执行：

```powershell
mcpp build --target x86_64-windows-gnu

$Built = Get-ChildItem .\target -Recurse -Filter mcpp.exe |
    Where-Object { $_.FullName -match '[\\/]bin[\\/]' } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $Built) { throw '没有找到新构建的 mcpp.exe' }
& $Built.FullName --version
```

将日志中的 resolved target/toolchain 和 `$Built.FullName` 记入测试结果。该 `.exe` 必须由 Windows 主机直接运行。

如果机器具备 Visual Studio C++ 工作负载，再执行一次 `mcpp build`，找到最新的 `mcpp.exe`，重复下述测试；如果不具备，只记录未测试，不要为此改系统环境。

## 3. 在源码树外实听

以下脚本创建独立临时工程，并复制待测 `.exe`，用于证明运行时不读取仓库里的 `assets/`：

```powershell
$TestRoot = Join-Path $env:TEMP ('mcpp-win-niulai-' + [guid]::NewGuid())
$Project = Join-Path $TestRoot 'project'
$Source = Join-Path $Project 'src'
New-Item -ItemType Directory -Force $Source | Out-Null
$Tool = Join-Path $TestRoot 'mcpp-niulai.exe'
Copy-Item $Built.FullName $Tool

[IO.File]::WriteAllText(
    (Join-Path $Project 'mcpp.toml'),
    "[package]`r`nname = `"niulai-win-smoke`"`r`nversion = `"0.1.0`"`r`n",
    [Text.Encoding]::ASCII)
[IO.File]::WriteAllText(
    (Join-Path $Source 'main.c'),
    "int main(void) { return 0; }`r`n",
    [Text.Encoding]::ASCII)

$Before = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::OrdinalIgnoreCase)
Get-ChildItem $env:TEMP -Filter 'mcpp-niulai-*-*.mp3' -ErrorAction SilentlyContinue |
    ForEach-Object { [void]$Before.Add($_.FullName) }

Push-Location $Project
try {
    & $Tool build --target x86_64-windows-gnu --niulai
    if ($LASTEXITCODE -ne 0) { throw '正常源码构建失败' }
    # 此处应实际听到 mama1.mp3 或 mama2.mp3。

    [IO.File]::WriteAllText(
        (Join-Path $Source 'main.c'),
        "int main(void) { this will not compile }`r`n",
        [Text.Encoding]::ASCII)
    & $Tool build --target x86_64-windows-gnu --niulai
    if ($LASTEXITCODE -eq 0) { throw '错误源码意外构建成功' }
    # 此处应实际听到 reply.mp3。

    [IO.File]::WriteAllText(
        (Join-Path $Source 'main.c'),
        "int main(void) { return 0; }`r`n",
        [Text.Encoding]::ASCII)
    & $Tool build --target x86_64-windows-gnu
    if ($LASTEXITCODE -ne 0) { throw '无 --niulai 的构建失败' }
    # 此处应保持静默。

    & $Tool build --target x86_64-windows-gnu --niulai --configure-only
    if ($LASTEXITCODE -ne 0) { throw 'configure-only 失败' }
    # 此处也应保持静默。
}
finally {
    Pop-Location
}

$Leaked = @(Get-ChildItem $env:TEMP -Filter 'mcpp-niulai-*-*.mp3' -ErrorAction SilentlyContinue |
    Where-Object { -not $Before.Contains($_.FullName) })
if ($Leaked) {
    $Leaked | Select-Object FullName, Length, LastWriteTime
    throw '检测到未清理的牛来临时 MP3'
}
```

不要只凭命令返回成功判断音频通过；请让现场用户确认两次实听内容。

## 4. 失败时收集证据

先保留完整输出，不要立即修改实现或安装播放器：

```powershell
$env:MCPP_LOG_LEVEL = 'debug'
Get-Command powershell.exe | Format-List *
Add-Type -AssemblyName PresentationCore
Test-Path $env:TEMP
& $Tool build --target x86_64-windows-gnu --niulai
Remove-Item Env:MCPP_LOG_LEVEL
```

区分以下阶段：Windows 原生 mcpp 编译失败、临时 MP3 无法写入、`PresentationCore`/`MediaPlayer` 失败、播放超时、声音设备无输出。记录原始报错和退出码后再判断是否需要改代码。

# 验收结果模板

```text
Windows 版本 / 架构：
PowerShell 版本：
bootstrap mcpp 版本：
待测 mcpp.exe 路径：
target / toolchain：
Windows 原生构建：通过 / 失败
成功构建：退出码 0；听到 mama1/mama2：是 / 否
失败构建：退出码非 0；听到 reply：是 / 否
无 --niulai：静默 / 异常
--configure-only：静默 / 异常
临时 MP3：无泄漏 / 泄漏路径
完整警告或错误：
```

# 已有验证与风险

- Linux glibc、Linux musl 静态构建、定向 E2E 和完整 `mcpp test` 均已通过；完整测试结果为 92 passed、0 failed。
- WSL 调用 Windows PowerShell `MediaPlayer` 已实际播放成功，但尚未验证 Windows 原生编译出的 `mcpp.exe`。
- 当前 shell E2E 在 Windows 会跳过，因此本轮必须按上面的 PowerShell 流程验证。
- 音频来自 whitefirer（MortyWang）的 `dsh-niulai-pet`；公开发布前仍需取得再分发许可。

# 推荐下一步

Windows Codex 只执行并报告上述测试。若失败，先提交环境、命令、退出码和完整日志；不要在没有复现结论时扩大修改范围。
