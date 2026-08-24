// mcpp.platform.niulai — best-effort build-result audio.

export module mcpp.platform.niulai;

import std;
import mcpp.platform.common;
import mcpp.platform.fs;
import mcpp.platform.process;

export namespace mcpp::platform::niulai {

bool announce(bool succeeded) noexcept;

} // namespace mcpp::platform::niulai

namespace mcpp::platform::niulai {

namespace {

std::optional<std::filesystem::path> find_powershell() {
    if (auto found = mcpp::platform::fs::which("powershell.exe")) return found;
    if constexpr (mcpp::platform::is_linux) {
        const std::filesystem::path system =
            "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe";
        if (std::filesystem::is_regular_file(system)) return system;
    }
    return std::nullopt;
}

std::string powershell_string(std::string_view value) {
    std::string quoted = "'";
    for (char c : value) quoted += c == '\'' ? "''" : std::string(1, c);
    return quoted + "'";
}

bool play_with_powershell(const std::filesystem::path& powershell,
                          std::string_view audio) {
    const auto script = std::format(
        "Add-Type -AssemblyName PresentationCore; "
        "$p=New-Object System.Windows.Media.MediaPlayer; "
        "$p.Open([Uri]::new({})); "
        "for($i=0;$i -lt 100 -and -not $p.NaturalDuration.HasTimeSpan;$i++)"
        "{{Start-Sleep -Milliseconds 50}}; "
        "if(-not $p.NaturalDuration.HasTimeSpan){{exit 1}}; "
        "$p.Play(); "
        "Start-Sleep -Milliseconds "
        "([int][Math]::Ceiling($p.NaturalDuration.TimeSpan.TotalMilliseconds)+100); "
        "$p.Close()",
        powershell_string(audio));
    return mcpp::platform::process::capture_exec({
        powershell.string(), "-NoProfile", "-NonInteractive", "-Command", script,
    }).exit_code == 0;
}

bool play_with_windows_host(const std::filesystem::path& powershell,
                            const std::filesystem::path& audio) {
    const std::filesystem::path windowsTemp = "/mnt/c/Windows/Temp";
    if (!std::filesystem::is_directory(windowsTemp)) return false;

    const auto name = std::format("mcpp-niulai-{:08x}{:08x}.mp3",
                                  std::random_device{}(), std::random_device{}());
    const auto staged = windowsTemp / name;
    std::error_code ec;
    std::filesystem::copy_file(audio, staged, ec);
    if (ec) return false;

    const bool played = play_with_powershell(
        powershell, "C:\\Windows\\Temp\\" + name);
    std::filesystem::remove(staged, ec);
    return played;
}

std::optional<std::filesystem::path> find_audio(std::string_view name) {
    auto dir = mcpp::platform::fs::self_exe_path().parent_path();
    while (!dir.empty()) {
        for (const auto& base : {
                 dir / "share" / "mcpp" / "niulai",
                 dir / "assets" / "niulai",
             }) {
            auto candidate = base / name;
            if (std::filesystem::is_regular_file(candidate)) return candidate;
        }
        auto parent = dir.parent_path();
        if (parent == dir) break;
        dir = std::move(parent);
    }
    return std::nullopt;
}

bool play_audio(const std::filesystem::path& audio) {
    if constexpr (mcpp::platform::is_windows) {
        if (auto powershell = find_powershell())
            return play_with_powershell(*powershell, audio.string());
    } else if constexpr (mcpp::platform::is_macos) {
        if (auto afplay = mcpp::platform::fs::which("afplay"))
            return mcpp::platform::process::capture_exec({
                afplay->string(), audio.string(),
            }).exit_code == 0;
    } else if constexpr (mcpp::platform::is_linux) {
        if (auto ffplay = mcpp::platform::fs::which("ffplay"))
            return mcpp::platform::process::capture_exec({
                ffplay->string(), "-nodisp", "-autoexit", "-loglevel", "quiet",
                audio.string(),
            }).exit_code == 0;
        if (auto mpv = mcpp::platform::fs::which("mpv"))
            return mcpp::platform::process::capture_exec({
                mpv->string(), "--no-video", "--really-quiet", audio.string(),
            }).exit_code == 0;
        if (auto mpg123 = mcpp::platform::fs::which("mpg123"))
            return mcpp::platform::process::capture_exec({
                mpg123->string(), "-q", audio.string(),
            }).exit_code == 0;
        if (auto powershell = find_powershell())
            return play_with_windows_host(*powershell, audio);
    }
    return false;
}

bool speak_with_powershell(const std::filesystem::path& powershell,
                           bool succeeded) {
    const auto phrase = succeeded
        ? "[string][char]0x5988"
        : "([string][char]0x725B)+[char]0x6765";
    const auto script = std::format(
        "Add-Type -AssemblyName System.Speech; "
        "$v=New-Object System.Speech.Synthesis.SpeechSynthesizer; "
        "$v.Speak({})",
        phrase);
    return mcpp::platform::process::capture_exec({
        powershell.string(), "-NoProfile", "-NonInteractive", "-Command", script,
    }).exit_code == 0;
}

} // namespace

bool announce(bool succeeded) noexcept {
    try {
        const std::string_view name = !succeeded ? "reply.mp3"
            : (std::random_device{}() & 1u) ? "mama1.mp3" : "mama2.mp3";
        if (auto audio = find_audio(name); audio && play_audio(*audio)) return true;

        if constexpr (mcpp::platform::is_windows) {
            if (auto powershell = find_powershell())
                return speak_with_powershell(*powershell, succeeded);
        } else if constexpr (mcpp::platform::is_macos) {
            if (auto say = mcpp::platform::fs::which("say"))
                return mcpp::platform::process::capture_exec({
                    say->string(), succeeded ? "妈" : "牛来",
                }).exit_code == 0;
        } else if constexpr (mcpp::platform::is_linux) {
            if (auto powershell = find_powershell())
                return speak_with_powershell(*powershell, succeeded);
            if (auto spdSay = mcpp::platform::fs::which("spd-say"))
                return mcpp::platform::process::capture_exec({
                    spdSay->string(), succeeded ? "妈" : "牛来",
                }).exit_code == 0;
        }
    } catch (...) {
    }
    return false;
}

} // namespace mcpp::platform::niulai
