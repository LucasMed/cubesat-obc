#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "comm_init.h"
#include <csp/csp.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/drivers/usart.h>

// Mock CSP functions used in comm_init
int init_called = 0;
void csp_init(void) {
    init_called = 1;
}

int usart_called = 0;

int usart_mock_return = CSP_ERR_NONE;
int csp_usart_open_and_add_kiss_interface(const csp_usart_conf_t *conf, const char *ifname, uint16_t node, csp_iface_t **iface) {
    usart_called++;
    assert(strcmp(conf->device, "uart1") == 0);
    assert(conf->baudrate == 115200);
    assert(strcmp(ifname, "KISS") == 0);
    assert(node == 10);
    return usart_mock_return;
}

static int rtable_called = 0;
int csp_rtable_load(const char *table) {
    rtable_called++;
    // Expected route: "1/255 KISS"
    assert(strcmp(table, "1/255 KISS") == 0);
    return CSP_ERR_NONE;
}

void reset_mocks() {
    init_called = 0;
    usart_called = 0;
    rtable_called = 0;
    usart_mock_return = CSP_ERR_NONE;
}

void test_comm_init_success() {
    reset_mocks();
    comm_init();
    assert(init_called == 1);
    assert(usart_called == 1);
    assert(rtable_called == 1);
    printf("test_comm_init_success PASS\n");
}

void test_comm_init_failure() {
    reset_mocks();
    usart_mock_return = CSP_ERR_DRIVER;
    comm_init();
    assert(init_called == 1);
    assert(usart_called == 1);
    assert(rtable_called == 0); // Should return early
    printf("test_comm_init_failure PASS\n");
}

int main() {
    printf("Running Comm Init tests...\n");
    test_comm_init_success();
    test_comm_init_failure();
    printf("All tests passed!\n");
    return 0;
}
