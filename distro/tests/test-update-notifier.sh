#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DISTRO=$(CDPATH= cd -- "$HERE/.." && pwd)
NOTIFIER="$DISTRO/config/includes.chroot/usr/local/bin/gnuchanos-update-notifier"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

mkdir -p "$tmp/bin" "$tmp/state"
cat > "$tmp/bin/apt" <<'EOF'
#!/bin/sh
printf '%s\n' 'Listing... Done'
printf '%s\n' "${APT_FIXTURE:-}"
EOF
cat > "$tmp/bin/notify-send" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$NOTIFY_LOG"
EOF
chmod +x "$tmp/bin/apt" "$tmp/bin/notify-send"

export PATH="$tmp/bin:/usr/bin:/bin"
export XDG_STATE_HOME="$tmp/state"
export NOTIFY_LOG="$tmp/notifications.log"
export APT_FIXTURE='alpha/stable 1.0 amd64 [upgradable from: 0.9]'

"$NOTIFIER"
"$NOTIFIER"
[ "$(wc -l < "$NOTIFY_LOG")" -eq 1 ]

export APT_FIXTURE='alpha/stable 1.0 amd64 [upgradable from: 0.9]
beta/stable 2.0 amd64 [upgradable from: 1.9]'
"$NOTIFIER"
[ "$(wc -l < "$NOTIFY_LOG")" -eq 2 ]

export APT_FIXTURE=
"$NOTIFIER"
export APT_FIXTURE='alpha/stable 1.0 amd64 [upgradable from: 0.9]'
"$NOTIFIER"
[ "$(wc -l < "$NOTIFY_LOG")" -eq 3 ]

printf '%s\n' 'Debian update notification tests passed'
