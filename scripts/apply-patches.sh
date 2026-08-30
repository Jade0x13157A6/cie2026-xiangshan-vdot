#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)
PROJECT_ROOT=$(
    cd "$SCRIPT_DIR/.."
    pwd
)

# shellcheck source=../versions.env
source "$PROJECT_ROOT/versions.env"

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /absolute/path/to/xs-env" >&2
    exit 2
fi

XS_ENV_PATH=$1

if [[ ! -d "$XS_ENV_PATH/.git" ]]; then
    echo "ERROR: not an xs-env Git repository: $XS_ENV_PATH" >&2
    exit 1
fi

apply_one()
{
    local label=$1
    local repo=$2
    local base=$3
    local patch=$4
    local head

    if [[ ! -d "$repo/.git" && ! -f "$repo/.git" ]]; then
        echo "ERROR: missing Git repository for $label: $repo" >&2
        exit 1
    fi

    if git -C "$repo" apply --reverse --check "$patch" \
        >/dev/null 2>&1
    then
        echo "ALREADY APPLIED: $label"
        return
    fi

    head=$(git -C "$repo" rev-parse HEAD)

    if [[ "$head" != "$base" ]]; then
        echo "ERROR: unexpected $label revision" >&2
        echo "  expected: $base" >&2
        echo "  actual:   $head" >&2
        echo "Refusing to patch an unknown source version." >&2
        exit 1
    fi

    if ! git -C "$repo" apply --check "$patch"; then
        echo "ERROR: patch check failed for $label" >&2
        exit 1
    fi

    git -C "$repo" apply "$patch"
    echo "APPLIED: $label"
}

apply_one \
    "yunsuan operation type" \
    "$XS_ENV_PATH/XiangShan/yunsuan" \
    "$YUNSUAN_BASE" \
    "$PROJECT_ROOT/patches/01-yunsuan-vdot.patch"

apply_one \
    "rocket-chip instruction encoding" \
    "$XS_ENV_PATH/XiangShan/rocket-chip" \
    "$ROCKET_CHIP_BASE" \
    "$PROJECT_ROOT/patches/02-rocket-chip-vdot.patch"

apply_one \
    "XiangShan decode and datapath" \
    "$XS_ENV_PATH/XiangShan" \
    "$XIANGSHAN_BASE" \
    "$PROJECT_ROOT/patches/03-xiangshan-vdot.patch"

apply_one \
    "NEMU reference model" \
    "$XS_ENV_PATH/NEMU" \
    "$NEMU_BASE" \
    "$PROJECT_ROOT/patches/04-nemu-vdot.patch"

apply_one \
    "nexus-am validation and benchmark" \
    "$XS_ENV_PATH/nexus-am" \
    "$NEXUS_AM_BASE" \
    "$PROJECT_ROOT/patches/05-nexus-am-tests.patch"

echo
echo "All VDOT patches are present."
echo
echo "Modified repositories:"

git -C "$XS_ENV_PATH/XiangShan/yunsuan" status --short --branch
git -C "$XS_ENV_PATH/XiangShan/rocket-chip" status --short --branch
git -C "$XS_ENV_PATH/XiangShan" status --short --branch
git -C "$XS_ENV_PATH/NEMU" status --short --branch
git -C "$XS_ENV_PATH/nexus-am" status --short --branch
