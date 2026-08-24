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

cat > "$TMP/bin/$player" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "${@: -1}" >> "$MCPP_NIULAI_TEST_LOG"
EOF
chmod +x "$TMP/bin/$player"

cat > "$TMP/project/mcpp.toml" <<'EOF'
[package]
name = "niulai-smoke"
version = "0.1.0"
EOF
cat > "$TMP/project/src/main.c" <<'EOF'
int main(void) { return 0; }
EOF

cd "$TMP/project"
"$MCPP" build --niulai >/dev/null 2>&1
case "$(basename "$(cat "$speaker_log")")" in
    mama1.mp3|mama2.mp3) ;;
    *) echo "successful build did not play a 妈妈 clip"; exit 1 ;;
esac

cat > src/main.c <<'EOF'
int main(void) { this will not compile }
EOF
rc=0
"$MCPP" build --niulai >/dev/null 2>&1 || rc=$?
[[ $rc -ne 0 ]] || { echo "broken source unexpectedly built"; exit 1; }
[[ "$(basename "$(tail -n 1 "$speaker_log")")" == "reply.mp3" ]] \
    || { echo "failed build did not play the 牛来 reply"; exit 1; }
[[ "$(wc -l < "$speaker_log")" -eq 2 ]] \
    || { echo "build result was announced more than once"; exit 1; }

: > "$speaker_log"
"$MCPP" build --niulai --configure-only >/dev/null 2>&1
[[ ! -s "$speaker_log" ]] \
    || { echo "configure-only unexpectedly triggered speech"; exit 1; }

echo "OK"
