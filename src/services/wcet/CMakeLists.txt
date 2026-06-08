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

if(PICO_BUILD)
    set(WCET_SRC wcet_profiler_pico.c)
else()
    set(WCET_SRC wcet_profiler_host.c)
endif()

add_library(wcet_lib ${WCET_SRC})

target_include_directories(wcet_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

target_compile_definitions(wcet_lib PUBLIC WCET_ENABLED=1)

# Propagate to the firmware target's include path so that ALL translation
# units (obc_main.c, health_monitor_task.c, etc.) see the real function
# declarations instead of the static inline no-ops from the header's #else.

# On host builds wcet_lib compiles wcet_profiler_host.c (guarded by #ifdef
# WCET_ENABLED) which provides safe stubs — no DWT hardware dependency.
