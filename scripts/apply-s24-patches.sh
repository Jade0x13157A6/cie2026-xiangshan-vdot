#!/usr/bin/env bash
# Apply S24 patch set against its recorded upstream revisions.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
P="$ROOT/patches/s24"
if [[ $# != 1 ]]; then
  echo "Usage: bash scripts/apply-s24-patches.sh /path/to/xs-env" >&2
  exit 2
fi
E="$(realpath "$1")"
[[ -d "$E" ]] || { echo "ERROR: xs-env directory missing" >&2; exit 1; }
(cd "$P" && sha256sum -c SHA256SUMS)
[[ "$(git -C "$E" rev-parse HEAD)" == "$(cat "$P/xs-env-base.txt")" ]] || {
  echo "ERROR: xs-env base revision does not match patches/s24/xs-env-base.txt" >&2; exit 1;
}
repo_for() {
  case "$1" in
    xiangshan) echo "$E/XiangShan" ;;
    yunsuan) echo "$E/XiangShan/yunsuan" ;;
    rocket-chip) echo "$E/XiangShan/rocket-chip" ;;
    nemu) echo "$E/NEMU" ;;
    nexus-am) echo "$E/nexus-am" ;;
  esac
}
names=(yunsuan rocket-chip xiangshan nemu nexus-am)
states=()
for name in "${names[@]}"; do
  repo="$(repo_for "$name")"
  patch="$P/$name.patch"
  if ! git -C "$repo" rev-parse HEAD >/dev/null 2>&1; then
    echo "ERROR: missing Git repository for $name: $repo" >&2; exit 1
  fi
  if [[ "$(git -C "$repo" rev-parse HEAD)" != "$(cat "$P/$name-base.txt")" ]]; then
    echo "ERROR: $name base revision mismatch" >&2; exit 1
  fi
  if git -C "$repo" apply --reverse --check "$patch" >/dev/null 2>&1; then
    states+=(present)
    echo "ALREADY APPLIED: $name"
  elif git -C "$repo" apply --check "$patch" >/dev/null 2>&1; then
    states+=(ready)
    echo "READY: $name"
  else
    echo "ERROR: $name patch neither present nor safely applicable" >&2; exit 1
  fi
done
for i in "${!names[@]}"; do
  if [[ "${states[$i]}" == ready ]]; then
    name="${names[$i]}"
    git -C "$(repo_for "$name")" apply "$P/$name.patch"
    echo "APPLIED: $name"
  fi
done
echo "S24 PATCHES PRESENT: five repositories"
