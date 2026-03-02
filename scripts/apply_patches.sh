#!/usr/bin/env bash
# scripts/apply_patches.sh
# Apply local patches to third-party submodules.
#
# Called automatically by .devcontainer/post-create.sh after
# 'git submodule update --init --recursive'.
# Safe to run multiple times — already-applied patches are skipped.

set -e
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

apply_patches_to() {
    local submodule_path="$1"
    local patch_dir="$REPO_ROOT/patches/$2"
    local abs_path="$REPO_ROOT/$submodule_path"

    if [ ! -d "$patch_dir" ]; then
        return 0
    fi

    echo "→ Applying patches to $submodule_path ..."
    for patch in "$patch_dir"/*.patch; do
        [ -f "$patch" ] || continue
        patch_name="$(basename "$patch")"

        # Forward check: if the patch *can* be applied cleanly it has not been
        # applied yet.  If it cannot apply (exit non-0) it is already in the
        # working tree — skip it.
        if ! git -C "$abs_path" apply --check "$patch" 2>/dev/null; then
            echo "  [skip] $patch_name (already applied)"
        else
            echo "  [apply] $patch_name"
            git -C "$abs_path" apply "$patch"
        fi
    done
}

apply_patches_to "third_party/libcsp" "libcsp"

echo "✓ Patches applied."
