#!/bin/sh

set -eu

srcdir=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/xsecurelock-version-c.XXXXXX")
status=0
trap 'status=$?; rm -rf "$tmpdir"; exit $status' 0 1 2 3 15

payload='v1.0";__attribute__((constructor))int p(void){return 7;}int q=sizeof"'
version_c="$tmpdir/version.c"
harness_c="$tmpdir/harness.c"
harness_bin="$tmpdir/harness"

GIT_VERSION=$payload /bin/sh "$srcdir"/generate-version-c.sh "$version_c"

cat > "$harness_c" <<'EOF'
#include <stdio.h>

extern const char *const git_version;

int main(void) {
  puts(git_version);
  return 0;
}
EOF

${CC:-clang} -o "$harness_bin" "$version_c" "$harness_c"
actual_output=$("$harness_bin")

if [ "$actual_output" != "$payload" ]; then
  echo "error: escaped version output mismatch" >&2
  exit 1
fi

if grep -F '__attribute__((constructor))' "$version_c" >/dev/null 2>&1; then
  echo "error: generated version.c contains unescaped constructor payload" >&2
  exit 1
fi
