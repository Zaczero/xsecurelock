#!/bin/sh

export XSECURELOCK_NO_COMPOSITE=1
export XSECURELOCK_SAVER_NOTIFY_ON_AUTH_OPEN=1
export XSECURELOCK_TEST_SAVER_SIGNAL_FILE="$homedir/saver-auth-open.signals"
export XSECURELOCK_TEST_SAVER_PID_FILE="$homedir/saver-auth-open.pid"
export XSECURELOCK_SAVER="$homedir/saver_auth_open_probe"

cat > "$XSECURELOCK_SAVER" <<'EOF'
#!/bin/sh

set -eu

: > "$XSECURELOCK_TEST_SAVER_SIGNAL_FILE"

trap 'printf "USR2\n" >> "$XSECURELOCK_TEST_SAVER_SIGNAL_FILE"' USR2
trap 'exit 0' TERM INT HUP

printf '%d\n' "$$" > "$XSECURELOCK_TEST_SAVER_PID_FILE"

while :; do
  sleep 1 &
  wait $! || true
done
EOF
chmod +x "$XSECURELOCK_SAVER"
