#!/usr/bin/env bash
# run_linux.sh — convenience wrapper to run common tasks inside the
# Linux Docker container WITHOUT opening VS Code Dev Containers.
#
# Prerequisites:
#   • Docker Desktop (or Docker Engine) installed and running
#   • Run from the repo root: bash run_linux.sh <command>
#
# Commands:
#   build       – Configure CMake + build all targets
#   test        – Build + run all CTest tests (verbose)
#   coverage    – Build + tests + HTML coverage report
#   analysis    – Run cppcheck static analysis
#   clean       – Remove build/ directory
#   shell       – Drop into an interactive bash shell inside the container
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
        echo "Removing build/ directory..."
        rm -rf build/
        echo "Done."
        ;;
    shell)
        ${COMPOSE} run --rm shell
        ;;
    help|*)
        head -30 "$0" | grep '^#' | sed 's/^# *//'
        ;;
esac
