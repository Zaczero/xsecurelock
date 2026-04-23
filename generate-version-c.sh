#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 output-path" >&2
  exit 1
fi

output_path=$1
git_version=${GIT_VERSION-}

if [ -z "$git_version" ]; then
  if git_version=$(git describe --always --dirty 2>/dev/null); then
    :
  elif [ -f version.c ]; then
    if [ "$output_path" != "$(pwd)/version.c" ]; then
      cat version.c > "$output_path"
    fi
    exit 0
  else
    echo "version.c must exist in non-git builds." >&2
    exit 1
  fi
fi

escaped_git_version=$(
  LC_ALL=C printf '%s' "$git_version" |
    od -An -tx1 -v |
    tr -d ' \n' |
    sed 's/../\\x&/g'
)

printf 'const char *const git_version = "%s";\n' "$escaped_git_version" \
  > "$output_path"
