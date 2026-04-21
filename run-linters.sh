#!/bin/sh

set -eu

refresh_configured_headers() {
  if [ -x ./config.status ]; then
    ./config.status config.h build-config.h >/dev/null
    return
  fi
  if [ ! -f ./config.h ] || [ ! -f ./build-config.h ]; then
    echo "error: run ./configure first so config.h and build-config.h exist" >&2
    exit 1
  fi
}

refresh_configured_headers

# clang-tidy.
if command -v clang-tidy >/dev/null 2>&1; then
  set -- \
    'bugprone-*' \
    'cert-*' \
    'clang-analyzer-*' \
    'misc-*' \
    'performance-*' \
    'readability-*' \
    '-cert-env33-c' \
    '-cert-msc30-c' \
    '-cert-msc50-cpp' \
    '-clang-analyzer-alpha.core.FixedAddr' \
    '-clang-analyzer-alpha.core.PointerArithm' \
    '-clang-analyzer-alpha.deadcode.UnreachableCode'
  checks=$(echo "$*" | tr ' ' ,)
  set -- \
    "-extra-arg=-I$PWD" \
    '-extra-arg=-include' \
    "-extra-arg=$PWD/config.h" \
    '-extra-arg=-include' \
    "-extra-arg=$PWD/build-config.h"
  # Try once without optional font includes.
  clang-tidy -checks="$checks" \
    "$@" \
    ./*.[ch] ./*/*.[ch]
  # Try again with all extensions.
  clang-tidy -checks="$checks" \
    -extra-arg=-I/usr/include/freetype2 \
    "$@" \
    ./*.[ch] ./*/*.[ch]
fi

# CPPCheck.
if command -v cppcheck >/dev/null 2>&1; then
  cppcheck --enable=all --inconclusive --std=posix  .
fi

# Clang Analyzer.
if command -v scan-build >/dev/null 2>&1; then
  make clean
  scan-build make
fi

# Build for Coverity Scan.
if command -v cov-build >/dev/null 2>&1; then
  make clean
  rm -rf cov-int
  cov-build --dir cov-int make
  tar cvjf cov-int.tbz2 cov-int/
  rm -rf cov-int
  rev=$(git describe --always --dirty)
  curl --form token="$COVERITY_TOKEN" \
    --form email="$COVERITY_EMAIL" \
    --form file=@cov-int.tbz2 \
    --form version="$rev" \
    --form description="$rev" \
    https://scan.coverity.com/builds?project=xsecurelock
  rm -f cov-int.tbz2
fi
