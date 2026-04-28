# WCET Profiler CMakeLists.txt
#
# Platform selection via PICO_BUILD:
#   ON  : wcet_profiler_pico.c   (ARM Cortex-M33 DWT cycle counter)
#   OFF : wcet_profiler_host.c   (no-ops for host unit tests / firmware)
#
# The wcet_lib library is linked into the main Pico firmware to enable
# WCET measurement via DWT->CYCCNT (CDR-HW-06, OI-3).
#
# On host builds the library is still built (as no-ops) so that any code
# that calls wcet_profiler_*() compiles without modification.
#
# CDR ref: CDR-HW-06, OI-3 (FSW-SDD-001 §14), OI-3 (OBC-DES-001 §15)

# TEMPORARY: WCET disabled via header (WCET_ENABLED not defined).
# Build with a dummy source file to satisfy CMake's no-empty-library rule.
# All profiling functions are static inline no-ops in the header.
#
# To re-enable: remove wcet_dummy.c and uncomment the two .c files below.

add_library(wcet_lib
    wcet_dummy.c
    # wcet_profiler_pico.c
    # wcet_profiler_host.c
)

target_include_directories(wcet_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
