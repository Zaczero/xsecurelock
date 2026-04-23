#!/bin/sh

set -e

script_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)

cd "$script_dir"
rm -f ./*.log

if [ "$#" -eq 0 ]; then
  set -- *.xdo
else
  original_count=$#
  for arg in "$@"; do
    test_name=${arg##*/}
    case "$test_name" in
      *.xdo) ;;
      *) test_name="$test_name.xdo" ;;
    esac
    if [ ! -f "$test_name" ]; then
      echo "Unknown test: $arg" >&2
      exit 1
    fi
    set -- "$@" "$test_name"
  done
  shift "$original_count"
fi

if [ "$#" -eq 0 ]; then
  echo "No matching tests." >&2
  exit 1
fi

xephyr=$(command -v Xephyr) || {
  echo "Xephyr is required to run the XDO tests." >&2
  exit 1
}
startx_bin=$(command -v startx) || {
  echo "startx is required to run the XDO tests." >&2
  exit 1
}
xdotool_bin=$(command -v xdotool) || {
  echo "xdotool is required to run the XDO tests." >&2
  exit 1
}
htpasswd_bin=$(command -v htpasswd) || {
  echo "htpasswd is required to run the XDO tests." >&2
  exit 1
}

case " $* " in
  *" test-xrandr.xdo "*) 
    command -v xwininfo >/dev/null 2>&1 || {
      echo "xwininfo is required for test-xrandr.xdo." >&2
      exit 1
    }
    command -v xrandr >/dev/null 2>&1 || {
      echo "xrandr is required for test-xrandr.xdo." >&2
      exit 1
    }
    command -v import >/dev/null 2>&1 || {
      echo "ImageMagick import is required for test-xrandr.xdo." >&2
      exit 1
    }
    ;;
esac

export PATH
PATH=$(dirname "$startx_bin"):$PATH
PATH=$(dirname "$xdotool_bin"):$PATH
PATH=$(dirname "$htpasswd_bin"):$PATH

for test in "$@"; do
  "$startx_bin" \
    /bin/sh "$PWD"/run-test.sh "$test" \
    -- \
    "$xephyr" :42 -retro -screen 640x480 \
    2>&1 |\
  tee "$test.log" |\
  grep "^Test $test status: 0\\.$"
done
