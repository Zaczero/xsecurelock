#!/bin/sh

set -eu

srcdir=$1
maketool=${MAKE:-make}

for tool in clang startx Xephyr xdotool htpasswd; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "error: make check requires '$tool' in PATH" >&2
    exit 1
  fi
done

checksrcdir=$(mktemp -d -t xsecurelock-check-src.XXXXXX)
builddir=$(mktemp -d -t xsecurelock-check-build.XXXXXX)
prefix=$(mktemp -d -t xsecurelock-check-prefix.XXXXXX)
status=0
trap 'status=$?; rm -rf "$checksrcdir" "$builddir" "$prefix"; exit $status' 0 1 2 3 15
rm -rf "$checksrcdir"
"$maketool" distdir distdir="$checksrcdir"

GIT_VERSION=$(
  cd "$srcdir"
  git describe --always --dirty 2>/dev/null || true
)
export GIT_VERSION

echo "Configuring sanitizer smoke build in $builddir"
cd "$builddir"
CC=clang \
CFLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' \
LDFLAGS='-fsanitize=address,undefined' \
"$checksrcdir"/configure \
  --prefix="$prefix" \
  --without-pam \
  --with-default-auth-module=auth_x11 \
  --with-default-authproto-module=authproto_htpasswd
"$maketool"
"$maketool" install

export ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1'
export UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1'

echo "Running native smoke tests"
"$builddir"/buf_util_test
"$builddir"/rect_test
"$builddir"/authproto_bounds_test
"$builddir"/auth_title_test
"$builddir"/configured_command_test
"$builddir"/dimmer_bayer_test
"$builddir"/prompt_display_test
"$builddir"/prompt_glyph_test
"$builddir"/prompt_state_test
"$builddir"/explicit_bzero_test
"$builddir"/explicit_bzero_fallback_test
"$builddir"/retry_io_test
"$builddir"/retry_io_fallback_test
"$builddir"/signal_pipe_test
"$builddir"/indicator_text_test
"$builddir"/xkb_test
"$builddir"/mlock_page_test
"$builddir"/mlock_page_overflow_test
"$builddir"/prompt_random_test
"$builddir"/env_settings_test
"$builddir"/wait_pgrp_test
"$builddir"/xscreensaver_api_test
"$builddir"/unmap_all_test
/bin/sh "$srcdir"/test/version_c_escape_test.sh "$srcdir"
"$srcdir"/test/saver_xscreensaver_smoke.sh "$builddir"/helpers/saver_xscreensaver
printf 'P 7\nhunter2\n' > "$builddir"/cat_authproto.fixture
"$builddir"/cat_authproto < "$builddir"/cat_authproto.fixture > "$builddir"/cat_authproto.out
cmp -s "$builddir"/cat_authproto.fixture "$builddir"/cat_authproto.out || {
  echo "error: cat_authproto round-trip smoke failed" >&2
  exit 1
}

echo "Running XDO smoke tests"
cd "$srcdir"/test
PATH="$prefix/bin:$PATH" ./run-tests.sh \
  test-correct-password \
  test-wrong-password \
  test-sigusr2-starts-auth \
  test-global-saver-relative-override \
  test-authproto-static-info-advance \
  test-authproto-static-info-timeout
