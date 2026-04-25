#!/bin/sh

set -e

test_dir=$(CDPATH='' cd -- "$(dirname "$1")" && pwd)
test_name=$(basename "$1")

cd "$test_dir"

xsecurelock_bin=$(command -v xsecurelock) || {
  echo "xsecurelock must be available in PATH before running XDO tests." >&2
  exit 1
}

# Set up an isolated homedir with a fixed password.
homedir=$(mktemp -d "${TMPDIR:-/tmp}/xsecurelock-run-test.XXXXXX") || exit 1
trap 'rm -rf "$homedir"' EXIT
htpasswd -bc "$homedir/.xsecurelock.pw" "$USER" hunter2

# Run preparatory commands.
eval "$(grep '^#preexec ' "$test_name" | cut -d ' ' -f 2-)"
: "${XSECURELOCK_AUTH:=auth_x11}"
: "${XSECURELOCK_AUTHPROTO:=authproto_htpasswd}"
: "${XSECURELOCK_SAVER:=saver_blank}"

# Lock the screen - and wait for the lock to succeed.
mkfifo "$homedir"/lock.notify
HOME="$homedir" XSECURELOCK_AUTH="$XSECURELOCK_AUTH" XSECURELOCK_AUTHPROTO="$XSECURELOCK_AUTHPROTO" XSECURELOCK_SAVER="$XSECURELOCK_SAVER" \
  "$xsecurelock_bin" -- cat "$homedir"/lock.notify & pid=$!
echo "Waiting for lock..."
: > "$homedir"/lock.notify
echo "Locked."

# Run the test script.
set +e
XSECURELOCK_PID=$pid xdotool - < "$test_name"
result=$?
set -e

# Kill the lock, if remaining.
kill "$pid" || true

# Finish the test.
echo "Test $test_name status: $result."
