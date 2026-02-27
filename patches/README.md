# Third-party patches

This directory contains patches applied to git submodules after `git submodule update --init`.
They are applied automatically by `.devcontainer/post-create.sh` and `scripts/apply_patches.sh`.

## Why patches instead of a fork?

The submodules are pinned to official upstream releases. Rather than maintaining a fork,
small targeted fixes are kept as patch files so contributors can see exactly what changed
and why. When upstream merges the fixes, the patch can simply be removed.

## libcsp

| File | Description |
|------|-------------|
| `libcsp/0001-linux-posix-compat.patch` | Adds `csp_hooks.c` to the POSIX arch (missing weak-symbol implementations of `csp_memfree_hook`, `csp_ps_hook`, `csp_reboot_hook`, `csp_shutdown_hook`), and several minor fixes to make libcsp build cleanly on Linux with GCC. |

## Applying manually

```bash
bash scripts/apply_patches.sh
```
