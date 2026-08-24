// mcpp.platform.niulai — best-effort build-result audio.

export module mcpp.platform.niulai;

import std;
import mcpp.platform.common;
import mcpp.platform.fs;
import mcpp.platform.niulai.assets;
import mcpp.platform.process;

export namespace mcpp::platform::niulai {

bool announce(bool succeeded) noexcept;

} // namespace mcpp::platform::niulai

namespace mcpp::platform::niulai {

namespace {

constexpr auto player_timeout = std::chrono::milliseconds{5000};

bool run_tool(const std::vector<std::string>& argv) {
    bool timed_out = false;
    const auto result = mcpp::platform::process::capture_exec_deadline(
        argv, {}, player_timeout, &timed_out);
    return !timed_out && result.exit_code == 0;
}

struct temporary_file {
    std::filesystem::path path;

    explicit temporary_file(std::filesystem::path value)
        : path(std::move(value)) {}
    temporary_file(temporary_file&& other) noexcept
        : path(std::exchange(other.path, {})) {}
    temporary_file(const temporary_file&) = delete;
    temporary_file& operator=(const temporary_file&) = delete;
    temporary_file& operator=(temporary_file&&) = delete;

    ~temporary_file() {
        if (path.empty()) return;
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

std::filesystem::path temporary_path(std::string_view suffix) {
    return std::filesystem::temp_directory_path()
        / std::format("mcpp-niulai-{:08x}{:08x}-{}",
                      std::random_device{}(), std::random_device{}(), suffix);
}

std::optional<temporary_file> write_audio(
    mcpp::platform::niulai::assets::clip audio) {
    auto path = temporary_path(audio.name);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return std::nullopt;
    output.write(audio.bytes.data(),
                 static_cast<std::streamsize>(audio.bytes.size()));
    output.close();
    if (!output) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return std::nullopt;
    }
    return temporary_file{std::move(path)};
}

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
    return run_tool({
        powershell.string(), "-NoProfile", "-NonInteractive", "-Command", script,
    });
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

std::optional<temporary_file> decode_wav(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& audio) {
    temporary_file wav{temporary_path("decoded.wav")};
    if (!run_tool({
            ffmpeg.string(), "-nostdin", "-hide_banner", "-loglevel", "error",
            "-y", "-i", audio.string(), "-vn", "-acodec", "pcm_s16le",
            wav.path.string(),
        }))
        return std::nullopt;
    if (!std::filesystem::is_regular_file(wav.path)) return std::nullopt;
    return std::move(wav);
}

bool play_audio(const std::filesystem::path& audio) {
    if constexpr (mcpp::platform::is_windows) {
        if (auto powershell = find_powershell())
            if (play_with_powershell(*powershell, audio.string())) return true;
        if (auto ffplay = mcpp::platform::fs::which("ffplay"))
            if (run_tool({
                    ffplay->string(), "-nodisp", "-autoexit", "-loglevel", "quiet",
                    audio.string(),
                }))
                return true;
    } else if constexpr (mcpp::platform::is_macos) {
        if (auto afplay = mcpp::platform::fs::which("afplay"))
            if (run_tool({afplay->string(), audio.string()})) return true;
        if (auto ffplay = mcpp::platform::fs::which("ffplay"))
            if (run_tool({
                    ffplay->string(), "-nodisp", "-autoexit", "-loglevel", "quiet",
                    audio.string(),
                }))
                return true;
    } else if constexpr (mcpp::platform::is_linux) {
        if (auto ffplay = mcpp::platform::fs::which("ffplay"))
            if (run_tool({
                    ffplay->string(), "-nodisp", "-autoexit", "-loglevel", "quiet",
                    audio.string(),
                }))
                return true;

        if (auto ffmpeg = mcpp::platform::fs::which("ffmpeg")) {
            const auto prefix = std::vector<std::string>{
                ffmpeg->string(), "-nostdin", "-hide_banner", "-loglevel", "error",
                "-i", audio.string(),
            };
            auto command = prefix;
            command.insert(command.end(), {"-f", "pulse", "mcpp-niulai"});
            if (run_tool(command)) return true;

            command = prefix;
            command.insert(command.end(), {"-f", "alsa", "default"});
            if (run_tool(command)) return true;

            if (auto wav = decode_wav(*ffmpeg, audio)) {
                if (auto pwPlay = mcpp::platform::fs::which("pw-play"))
                    if (run_tool({pwPlay->string(), wav->path.string()})) return true;
                if (auto paPlay = mcpp::platform::fs::which("paplay"))
                    if (run_tool({paPlay->string(), wav->path.string()})) return true;
                if (auto aplay = mcpp::platform::fs::which("aplay"))
                    if (run_tool({aplay->string(), "-q", wav->path.string()})) return true;
            }
        }

        if (auto mpv = mcpp::platform::fs::which("mpv"))
            if (run_tool({
                    mpv->string(), "--no-video", "--really-quiet", audio.string(),
                }))
                return true;
        if (auto mpg123 = mcpp::platform::fs::which("mpg123"))
            if (run_tool({mpg123->string(), "-q", audio.string()})) return true;
        if (auto powershell = find_powershell())
            if (play_with_windows_host(*powershell, audio)) return true;
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
    return run_tool({
        powershell.string(), "-NoProfile", "-NonInteractive", "-Command", script,
    });
}

} // namespace

bool announce(bool succeeded) noexcept {
    try {
        const auto clip = !succeeded ? assets::reply()
            : (std::random_device{}() & 1u) ? assets::mama1() : assets::mama2();
        if (auto audio = write_audio(clip); audio && play_audio(audio->path)) return true;

        if constexpr (mcpp::platform::is_windows) {
            if (auto powershell = find_powershell())
                if (speak_with_powershell(*powershell, succeeded)) return true;
        } else if constexpr (mcpp::platform::is_macos) {
            if (auto say = mcpp::platform::fs::which("say"))
                if (run_tool({say->string(), succeeded ? "妈" : "牛来"})) return true;
        } else if constexpr (mcpp::platform::is_linux) {
            if (auto powershell = find_powershell())
                if (speak_with_powershell(*powershell, succeeded)) return true;
            if (auto spdSay = mcpp::platform::fs::which("spd-say"))
                if (run_tool({spdSay->string(), succeeded ? "妈" : "牛来"})) return true;
        }
    } catch (...) {
    }
    std::cerr << '\a' << std::flush;
    return false;
}

} // namespace mcpp::platform::niulai
