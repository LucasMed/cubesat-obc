#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

// Mock FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

uint32_t mock_tick_count = 1000;
uint32_t mock_xTaskGetTickCount(void) {
    return mock_tick_count;
}
void mock_vTaskDelayUntil(uint32_t *pxPreviousWakeTime, uint32_t xTimeIncrement) {
    *pxPreviousWakeTime += xTimeIncrement;
    mock_tick_count += xTimeIncrement;
}

#undef xTaskGetTickCount
#undef vTaskDelayUntil
#define xTaskGetTickCount mock_xTaskGetTickCount
#define vTaskDelayUntil mock_vTaskDelayUntil

// Mock vTaskDelay used in command_task
uint32_t last_delay = 0;
#undef vTaskDelay
void vTaskDelay(uint32_t ticks) {
    last_delay = ticks;
    mock_tick_count += ticks;
}

// Mock libcsp
#include <csp/csp.h>

int last_freed = 0;
void csp_buffer_free(void *packet) {
    last_freed++;
    free(packet);
}

csp_packet_t *mock_csp_buffer_get(size_t size) {
    csp_packet_t *packet = calloc(1, sizeof(csp_packet_t) + 256);
    packet->length = 0;
    return packet;
}
#define csp_buffer_get mock_csp_buffer_get

csp_packet_t *last_csp_sent = NULL;
void csp_send(csp_conn_t *conn, csp_packet_t *packet) {
    last_csp_sent = packet;
    // in real life csp_send takes ownership, so we free it to simulate that taking ownership
    free(packet);
}

int csp_conn_src(const csp_conn_t *conn) {
    return 1; // Simulated GN Address
}

// Actual file to test
#include "command_task.h"

void reset_mocks() {
    last_freed = 0;
    last_csp_sent = NULL;
    last_delay = 0;
}

void test_command_echo() {
    reset_mocks();
    csp_conn_t *mock_conn = NULL;
    csp_packet_t *pkt = csp_buffer_get(0);
    
    csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
    cmd->cmd_id = CMD_ECHO;
    strcpy((char*)cmd->payload, "HELLO");
    pkt->length = 1 + strlen("HELLO");

    process_command_packet(mock_conn, pkt);

    // Memory ownership passed to csp_send, which freed it.
    assert(last_csp_sent != NULL);
    assert(last_freed == 0); // No explicit free, csp_send took ownership
    printf("test_command_echo PASS\n");
}

void test_command_reboot() {
    reset_mocks();
    csp_conn_t *mock_conn = NULL;
    csp_packet_t *pkt = csp_buffer_get(0);
    
    csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
    cmd->cmd_id = CMD_REBOOT;
    pkt->length = 1;

    process_command_packet(mock_conn, pkt);

    assert(last_csp_sent == NULL);
    assert(last_freed == 1); // Freeed explicitly
    assert(last_delay == pdMS_TO_TICKS(100)); // Delay for logs to flush
    printf("test_command_reboot PASS\n");
}

void test_command_invalid() {
    reset_mocks();
    csp_conn_t *mock_conn = NULL;
    csp_packet_t *pkt = csp_buffer_get(0);
    
    csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
    cmd->cmd_id = 99; // unknown
    pkt->length = 1;

    process_command_packet(mock_conn, pkt);

    assert(last_csp_sent == NULL);
    assert(last_freed == 1); // Dropped and freed
    printf("test_command_invalid PASS\n");
}

void test_command_short_packet() {
    reset_mocks();
    csp_conn_t *mock_conn = NULL;
    csp_packet_t *pkt = csp_buffer_get(0);
    
    pkt->length = 0; // too short

    process_command_packet(mock_conn, pkt);

    assert(last_csp_sent == NULL);
    assert(last_freed == 1); // Dropped and freed
    printf("test_command_short_packet PASS\n");
}

int main() {
    printf("Running Command Task tests...\n");
    test_command_echo();
    test_command_reboot();
    test_command_invalid();
    test_command_short_packet();
    printf("All tests passed!\n");
    return 0;
}
