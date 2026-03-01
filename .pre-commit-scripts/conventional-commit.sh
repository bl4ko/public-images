#!/usr/bin/env bash
set -euo pipefail

msg=$(head -1 "$1")

# Allow merge commits
if echo "$msg" | grep -qE "^Merge "; then
  exit 0
fi

# Validate conventional commit format
if ! echo "$msg" | grep -qE "^(feat|fix|docs|style|refactor|test|chore|perf)(\([^)]+\))?: .+"; then
  echo "Invalid commit message: $msg"
  echo "Expected: <type>(<scope>): <subject>"
  echo "Types: feat|fix|docs|style|refactor|test|chore|perf"
  exit 1
fi
