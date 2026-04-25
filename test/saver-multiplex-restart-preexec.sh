#!/bin/sh

export XSECURELOCK_NO_COMPOSITE=1
export XSECURELOCK_TEST_SAVER_COUNT_FILE="$homedir/saver-multiplex-restarts.count"
export XSECURELOCK_SAVER="$homedir/saver_multiplex_exit_immediately"

cat > "$XSECURELOCK_SAVER" <<'EOF'
#!/bin/sh

set -eu

count=$(cat "$XSECURELOCK_TEST_SAVER_COUNT_FILE" 2>/dev/null || echo 0)
printf '%d\n' "$((count + 1))" > "$XSECURELOCK_TEST_SAVER_COUNT_FILE"
exit 1
EOF
chmod +x "$XSECURELOCK_SAVER"
