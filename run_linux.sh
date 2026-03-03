#!/usr/bin/env bash
# run_linux.sh — convenience wrapper to run common tasks inside the
# Linux Docker container WITHOUT opening VS Code Dev Containers.
#
# Prerequisites:
#   • Docker Desktop (or Docker Engine) installed and running
#   • Run from the repo root: bash run_linux.sh <command>
#
# Host-build commands (fast, no Pico SDK needed):
#   build       – Configure CMake + build all targets
#   test        – Build + run all CTest tests (verbose)
#   coverage    – Build + tests + HTML coverage report
#   analysis    – Run static analysis (cppcheck + clang-format + clang-tidy)
#   clean       – Remove build/ directory
#   shell       – Drop into an interactive bash shell inside the container
#
# Pico 2W hardware commands (uses docker/Dockerfile.pico image):
#   pico-test   – Full pipeline: host-tests → pico-build → emulate → .uf2
#                 Artifacts written to ./artifacts/ (includes flash-ready .uf2)
#   pico-build  – Cross-compile RP2350 .uf2 only (no tests)
#   pico-shell  – Interactive shell with Pico SDK + ARM toolchain + rp2040js
#
#   help        – Show this message

set -e

COMPOSE="docker compose"

case "${1:-help}" in
    build)
        ${COMPOSE} run --rm build
        ;;
    test)
        ${COMPOSE} run --rm test
        ;;
    coverage)
        ${COMPOSE} run --rm coverage
        ;;
    analysis)
        ${COMPOSE} run --rm shell bash -c "
            cppcheck \
                --enable=all \
                --suppress=missingIncludeSystem \
                --suppress=unusedFunction \
                -I include \
                -I third_party/libcsp/include \
                src/ tests/ 2>&1 | tee build/cppcheck.txt
            echo 'Static analysis complete. Report: build/cppcheck.txt'
        "
        ;;
    clean)
        echo "Removing build directories..."
        rm -rf build/ build_pico/ build_emu/
        echo "Done."
        ;;
    shell)
        ${COMPOSE} run --rm shell
        ;;

    # ── Pico 2W hardware pipeline ──────────────────────────────────────────
    pico-test)
        echo "=== CubeSat OBC — Pico 2W validation pipeline ==="
        echo "    Stages: host-tests → pico-build → emulate → static → coverage"
        echo "    Artifacts → ./artifacts/"
        mkdir -p artifacts
        ${COMPOSE} run --rm pico-test
        ;;
    pico-build)
        echo "=== CubeSat OBC — RP2350 firmware build ==="
        mkdir -p artifacts
        ${COMPOSE} run --rm pico-build
        echo "Flash artifact: ./artifacts/cubesat_obc_pico.uf2"
        ;;
    pico-shell)
        ${COMPOSE} run --rm pico-shell
        ;;

    help|*)
        head -40 "$0" | grep '^#' | sed 's/^# *//'
        ;;
esac
