/**
 * @file wcet_dummy.c
 * @brief Dummy source file for wcet_lib when WCET is disabled.
 *
 * This file exists solely to satisfy CMake's requirement that a library
 * must have at least one source file.  All WCET profiling is disabled
 * via the header (WCET_ENABLED not defined), so this file is empty.
 *
 * When re-enabling WCET: remove this file from CMakeLists.txt and
 * uncomment wcet_profiler_pico.c / wcet_profiler_host.c.
 */

/* Intentionaly empty — WCET disabled via header */
