/**
 * @file system_state.c
 * @brief Thread-safe system state implementation.
 */

#include "system_state.h"
#include <string.h>

#ifdef PICO_BUILD
#include "FreeRTOS.h"
#include "semphr.h"
static SemaphoreHandle_t g_state_mutex = NULL;
#endif

static system_state_t g_state;

void system_state_init(void) {
    memset(&g_state, 0, sizeof(system_state_t));
    
#ifdef PICO_BUILD
    g_state_mutex = xSemaphoreCreateMutex();
#endif
}

static void lock_state(void) {
#ifdef PICO_BUILD
    if (g_state_mutex) xSemaphoreTake(g_state_mutex, portMAX_DELAY);
#endif
}

static void unlock_state(void) {
#ifdef PICO_BUILD
    if (g_state_mutex) xSemaphoreGive(g_state_mutex);
#endif
}

void system_state_set_available(bool imu, bool temp) {
    lock_state();
    g_state.imu_available = imu;
    g_state.temp_available = temp;
    unlock_state();
}

void system_state_set_imu(const float att[3], const float rates[3]) {
    lock_state();
    for (int i=0; i<3; i++) {
        g_state.attitude[i] = att[i];
        g_state.rates[i] = rates[i];
    }
    g_state.imu_valid = true;
    unlock_state();
}

void system_state_set_temp(float temp) {
    lock_state();
    g_state.temp = temp;
    g_state.temp_valid = true;
    unlock_state();
}

void system_state_get(system_state_t *out_state) {
    if (!out_state) return;
    lock_state();
    memcpy(out_state, &g_state, sizeof(system_state_t));
    unlock_state();
}
