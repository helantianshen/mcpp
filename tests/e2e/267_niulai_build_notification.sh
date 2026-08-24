#!/usr/bin/env bash
# requires: unix-shell
# `mcpp build --niulai` announces the aggregate build result once.
set -e

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

case "$(uname -s)" in
    Linux) player=ffplay ;;
    Darwin) player=afplay ;;
    *) echo "OK"; exit 0 ;;
esac

mkdir -p "$TMP/bin" "$TMP/project/src"
speaker_log="$TMP/spoken"
export MCPP_NIULAI_TEST_LOG="$speaker_log"
export PATH="$TMP/bin:$PATH"
export MCPP_HOME="${MCPP_HOME:-$HOME/.mcpp}"
cp "$MCPP" "$TMP/bin/mcpp-niulai-test"
chmod +x "$TMP/bin/mcpp-niulai-test"
MCPP_UNDER_TEST="$TMP/bin/mcpp-niulai-test"

cat > "$TMP/bin/$player" <<'EOF'
#!/usr/bin/env bash
audio="${@: -1}"
bytes=$(wc -c < "$audio" | tr -d '[:space:]')
printf 'primary\t%s\t%s\n' "$audio" "$bytes" >> "$MCPP_NIULAI_TEST_LOG"
[[ "${MCPP_NIULAI_FAIL_PRIMARY:-0}" != 1 ]]
EOF
chmod +x "$TMP/bin/$player"

if [[ "$(uname -s)" == Linux ]]; then
    cat > "$TMP/bin/ffmpeg" <<'EOF'
#!/usr/bin/env bash
format=
input=
output=
while (($#)); do
    output=$1
    case "$1" in
        -i) shift; input=$1 ;;
        -f) shift; format=$1 ;;
    esac
    shift
done
printf 'ffmpeg\t%s\t%s\n' "${format:-wav}" "$input" >> "$MCPP_NIULAI_TEST_LOG"
[[ -f "$input" ]] || exit 1
if [[ -z "$format" ]]; then
    cp "$input" "$output"
    exit 0
fi
[[ "${MCPP_NIULAI_FFMPEG_DEVICE_FAIL:-0}" != 1 && "$format" == alsa ]]
EOF
    chmod +x "$TMP/bin/ffmpeg"

    cat > "$TMP/bin/pw-play" <<'EOF'
#!/usr/bin/env bash
audio="${@: -1}"
printf 'pw-play\twav\t%s\n' "$audio" >> "$MCPP_NIULAI_TEST_LOG"
[[ -f "$audio" ]]
EOF
    chmod +x "$TMP/bin/pw-play"
fi

cat > "$TMP/project/mcpp.toml" <<'EOF'
[package]
name = "niulai-smoke"
version = "0.1.0"
EOF
cat > "$TMP/project/src/main.c" <<'EOF'
int main(void) { return 0; }
EOF

cd "$TMP/project"
"$MCPP_UNDER_TEST" build --niulai >/dev/null 2>&1
IFS=$'\t' read -r backend audio_path audio_size < "$speaker_log"
[[ "$backend" == primary ]] || { echo "successful build used the wrong player"; exit 1; }
case "$(basename "$audio_path"):$audio_size" in
    mcpp-niulai-*-mama1.mp3:56835|mcpp-niulai-*-mama2.mp3:50566) ;;
    *) echo "successful build did not play a 妈妈 clip"; exit 1 ;;
esac
[[ ! -e "$audio_path" ]] || { echo "temporary success audio was not removed"; exit 1; }

cat > src/main.c <<'EOF'
int main(void) { this will not compile }
EOF
rc=0
"$MCPP_UNDER_TEST" build --niulai >/dev/null 2>&1 || rc=$?
[[ $rc -ne 0 ]] || { echo "broken source unexpectedly built"; exit 1; }
IFS=$'\t' read -r backend audio_path audio_size <<< "$(tail -n 1 "$speaker_log")"
case "$(basename "$audio_path"):$audio_size" in
    mcpp-niulai-*-reply.mp3:5974) ;;
    *) echo "failed build did not play the 牛来 reply"; exit 1 ;;
esac
[[ ! -e "$audio_path" ]] || { echo "temporary failure audio was not removed"; exit 1; }
[[ "$(wc -l < "$speaker_log")" -eq 2 ]] \
    || { echo "build result was announced more than once"; exit 1; }

: > "$speaker_log"
"$MCPP_UNDER_TEST" build --niulai --configure-only >/dev/null 2>&1
[[ ! -s "$speaker_log" ]] \
    || { echo "configure-only unexpectedly triggered speech"; exit 1; }

if [[ "$(uname -s)" == Linux ]]; then
    cat > src/main.c <<'EOF'
int main(void) { return 0; }
EOF
    export MCPP_NIULAI_FAIL_PRIMARY=1
    "$MCPP_UNDER_TEST" build --niulai >/dev/null 2>&1
    unset MCPP_NIULAI_FAIL_PRIMARY
    [[ "$(cut -f1 "$speaker_log" | sed -n '1p')" == primary ]] \
        || { echo "ffplay was not attempted first"; exit 1; }
    [[ "$(cut -f1,2 "$speaker_log" | sed -n '2p')" == $'ffmpeg\tpulse' ]] \
        || { echo "FFmpeg PulseAudio was not attempted second"; exit 1; }
    [[ "$(cut -f1,2 "$speaker_log" | sed -n '3p')" == $'ffmpeg\talsa' ]] \
        || { echo "FFmpeg ALSA fallback was not attempted"; exit 1; }

    : > "$speaker_log"
    export MCPP_NIULAI_FAIL_PRIMARY=1
    export MCPP_NIULAI_FFMPEG_DEVICE_FAIL=1
    "$MCPP_UNDER_TEST" build --niulai >/dev/null 2>&1
    unset MCPP_NIULAI_FAIL_PRIMARY MCPP_NIULAI_FFMPEG_DEVICE_FAIL
    [[ "$(cut -f1,2 "$speaker_log" | sed -n '4p')" == $'ffmpeg\twav' ]] \
        || { echo "FFmpeg WAV conversion was not attempted"; exit 1; }
    [[ "$(cut -f1,2 "$speaker_log" | sed -n '5p')" == $'pw-play\twav' ]] \
        || { echo "PipeWire WAV fallback was not attempted"; exit 1; }
    wav_path=$(cut -f3 "$speaker_log" | sed -n '5p')
    [[ ! -e "$wav_path" ]] || { echo "temporary decoded audio was not removed"; exit 1; }
fi

echo "OK"
