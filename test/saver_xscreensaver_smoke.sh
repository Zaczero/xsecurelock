#!/bin/sh

set -eu

helper_dir=$(cd "$(dirname "$1")" && pwd)
helper=$helper_dir/$(basename "$1")

fail() {
  echo "error: $*" >&2
  exit 1
}

assert_file_content() {
  file=$1
  expected=$2

  [ -f "$file" ] || fail "missing file: $file"
  actual=$(cat "$file")
  [ "$actual" = "$expected" ] || {
    echo "error: unexpected file content in $file" >&2
    echo "expected: $expected" >&2
    echo "actual: $actual" >&2
    exit 1
  }
}

run_helper_once() {
  home_dir=$1
  saver_dir=$2
  pidfile=$3

  run_helper_once_in_dir . "$home_dir" "$saver_dir" "$pidfile"
}

run_helper_once_in_dir() {
  work_dir=$1
  home_dir=$2
  saver_dir=$3
  pidfile=$4

  (
    cd "$work_dir"
    XSECURELOCK_XSCREENSAVER_PATH="$saver_dir" \
      HOME="$home_dir" \
      "$helper"
  ) &
  helper_pid=$!
  printf '%s\n' "$helper_pid" > "$pidfile"

  status=0
  if ! wait "$helper_pid"; then
    status=$?
  fi
  case "$status" in
    0|143) ;;
    *) fail "unexpected helper exit status $status" ;;
  esac
}

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/xsecurelock-saver-xscreensaver.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT

grep -Fq "sh -c \"\$saver\"" "$helper" || fail "helper does not use sh -c"
if grep -Fq "eval \$saver" "$helper"; then
  fail "helper still uses eval"
fi

config_home=$tmpdir/home-config
config_saver_dir=$tmpdir/savers-config
mkdir -p "$config_home" "$config_saver_dir"
cat > "$config_home/.xscreensaver" <<'EOF'
mode:	one
selected:	0
programs:	\
	"Fake" fake-hack --label "hello world"\n
EOF
cat > "$config_saver_dir/fake-hack" <<'EOF'
#!/bin/sh
set -eu
fake-helper
printf '%s\n' "$#" > "$XSL_ARGC_FILE"
printf '%s\n' "$1" > "$XSL_ARG1_FILE"
printf '%s\n' "$2" > "$XSL_ARG2_FILE"
helper_pid=$(cat "$XSL_HELPER_PID_FILE")
kill "$helper_pid"
EOF
chmod +x "$config_saver_dir/fake-hack"
cat > "$config_saver_dir/fake-helper" <<'EOF'
#!/bin/sh
set -eu
printf 'found\n' > "$XSL_HELPER_LOOKUP_FILE"
EOF
chmod +x "$config_saver_dir/fake-helper"

XSL_ARGC_FILE=$tmpdir/config.argc \
XSL_ARG1_FILE=$tmpdir/config.arg1 \
XSL_ARG2_FILE=$tmpdir/config.arg2 \
XSL_HELPER_LOOKUP_FILE=$tmpdir/config.helper \
XSL_HELPER_PID_FILE=$tmpdir/config.pid \
  run_helper_once "$config_home" "$config_saver_dir" "$tmpdir/config.pid"

assert_file_content "$tmpdir/config.helper" "found"
assert_file_content "$tmpdir/config.argc" "2"
assert_file_content "$tmpdir/config.arg1" "--label"
assert_file_content "$tmpdir/config.arg2" "hello world"

fallback_home=$tmpdir/home-fallback
fallback_saver_dir=$tmpdir/savers-fallback
mkdir -p "$fallback_home" "$fallback_saver_dir"
cat > "$fallback_saver_dir/fallback-hack" <<'EOF'
#!/bin/sh
set -eu
printf '%s\n' "$#" > "$XSL_ARGC_FILE"
printf '%s\n' "$1" > "$XSL_ARG1_FILE"
helper_pid=$(cat "$XSL_HELPER_PID_FILE")
kill "$helper_pid"
EOF
chmod +x "$fallback_saver_dir/fallback-hack"

XSL_ARGC_FILE=$tmpdir/fallback.argc \
XSL_ARG1_FILE=$tmpdir/fallback.arg1 \
XSL_HELPER_PID_FILE=$tmpdir/fallback.pid \
  run_helper_once "$fallback_home" "$fallback_saver_dir" "$tmpdir/fallback.pid"

assert_file_content "$tmpdir/fallback.argc" "1"
assert_file_content "$tmpdir/fallback.arg1" "-root"

random_home=$tmpdir/home-random
random_saver_dir=$tmpdir/savers-random
mkdir -p "$random_home" "$random_saver_dir"
cat > "$random_home/.xscreensaver" <<'EOF'
mode:	random
programs:	\
	"Random" random-hack --random-mode\n
EOF
cat > "$random_saver_dir/random-hack" <<'EOF'
#!/bin/sh
set -eu
printf '%s\n' "$1" > "$XSL_RANDOM_ARG_FILE"
helper_pid=$(cat "$XSL_HELPER_PID_FILE")
kill "$helper_pid"
EOF
chmod +x "$random_saver_dir/random-hack"

XSL_RANDOM_ARG_FILE=$tmpdir/random.arg \
XSL_HELPER_PID_FILE=$tmpdir/random.pid \
  run_helper_once "$random_home" "$random_saver_dir" "$tmpdir/random.pid"

assert_file_content "$tmpdir/random.arg" "--random-mode"

empty_home=$tmpdir/home-empty
empty_saver_dir=$tmpdir/savers-empty
empty_work_dir=$tmpdir/work-empty
mkdir -p "$empty_home" "$empty_saver_dir" "$empty_work_dir"
cat > "$empty_work_dir/saver_blank" <<'EOF'
#!/bin/sh
set -eu
printf 'blank\n' > "$XSL_BLANK_FILE"
EOF
chmod +x "$empty_work_dir/saver_blank"

XSL_BLANK_FILE=$tmpdir/empty.blank \
  run_helper_once_in_dir "$empty_work_dir" "$empty_home" "$empty_saver_dir" \
    "$tmpdir/empty.pid"

assert_file_content "$tmpdir/empty.blank" "blank"
