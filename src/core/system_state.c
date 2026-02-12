#include "config.h"
#include <stdio.h>

// Minimal system state placeholder
typedef struct {
    float attitude[3]; // roll, pitch, yaw
    float rates[3];
} system_state_t;

static system_state_t g_state;

void system_state_init(void) {
    for (int i=0;i<3;i++) { g_state.attitude[i]=0.0f; g_state.rates[i]=0.0f; }
}

void system_state_get(float att[3], float rates[3]) {
    for (int i=0;i<3;i++) { att[i]=g_state.attitude[i]; rates[i]=g_state.rates[i]; }
}
