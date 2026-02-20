/**
 * @file pico_usart.c
 * @brief libcsp UART driver implementation for Raspberry Pi Pico (RP2350)
 */

#include <csp/drivers/usart.h>
#include <csp/csp_debug.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

#define UART_RX_TASK_STACK_SIZE 1024
#define UART_RX_TASK_PRIORITY   (configMAX_PRIORITIES - 1)

typedef struct {
    uart_inst_t * uart_inst;
    csp_usart_callback_t rx_callback;
    void * user_data;
    SemaphoreHandle_t lock;
    TaskHandle_t rx_task_handle;
} pico_usart_driver_t;

static pico_usart_driver_t driver_instance;

// Forward declaration of the RX task
static void uart_rx_task(void *pvParameters);

int csp_usart_open(const csp_usart_conf_t * conf, csp_usart_callback_t rx_callback, void * user_data, csp_usart_fd_t * fd) {
    
    // For simplicity, we assume UART1 is used if requested via device "uart1"
    if (strcmp(conf->device, "uart0") == 0) {
        driver_instance.uart_inst = uart0;
    } else {
        driver_instance.uart_inst = uart1;
    }

    driver_instance.rx_callback = rx_callback;
    driver_instance.user_data = user_data;
    
    // Initialize UART
    uart_init(driver_instance.uart_inst, conf->baudrate);
    
    // Hardcoded pins for now (Task 3.2 strategy)
    if (driver_instance.uart_inst == uart1) {
        gpio_set_function(4, GPIO_FUNC_UART);
        gpio_set_function(5, GPIO_FUNC_UART);
    } else {
        gpio_set_function(0, GPIO_FUNC_UART);
        gpio_set_function(1, GPIO_FUNC_UART);
    }

    uart_set_hw_flow(driver_instance.uart_inst, false, false);
    uart_set_format(driver_instance.uart_inst, conf->databits, conf->stopbits, conf->paritysetting);
    uart_set_fifo_enabled(driver_instance.uart_inst, true);

    // Create mutex for thread-safe writes
    driver_instance.lock = xSemaphoreCreateMutex();
    
    // Create RX task
    if (xTaskCreate(uart_rx_task, "UART_RX", UART_RX_TASK_STACK_SIZE, &driver_instance, UART_RX_TASK_PRIORITY, &driver_instance.rx_task_handle) != pdPASS) {
        return CSP_ERR_NOMEM;
    }

    if (fd) {
        // Return a dummy FD or the pointer to our driver instance
        *fd = 1; 
    }

    return CSP_ERR_NONE;
}

int csp_usart_write(csp_usart_fd_t fd, const void * data, size_t data_length) {
    (void)fd;
    const uint8_t * buf = (const uint8_t *)data;
    
    if (driver_instance.uart_inst == NULL) {
        return -1;
    }

    for (size_t i = 0; i < data_length; i++) {
        uart_putc_raw(driver_instance.uart_inst, buf[i]);
    }
    
    return (int)data_length;
}

void csp_usart_lock(void * driver_data) {
    (void)driver_data;
    if (driver_instance.lock) {
        xSemaphoreTake(driver_instance.lock, portMAX_DELAY);
    }
}

void csp_usart_unlock(void * driver_data) {
    (void)driver_data;
    if (driver_instance.lock) {
        xSemaphoreGive(driver_instance.lock);
    }
}

static void uart_rx_task(void *pvParameters) {
    pico_usart_driver_t * drv = (pico_usart_driver_t *)pvParameters;
    uint8_t rx_buf[1];

    while (1) {
        if (uart_is_readable(drv->uart_inst)) {
            rx_buf[0] = uart_getc(drv->uart_inst);
            if (drv->rx_callback) {
                drv->rx_callback(drv->user_data, rx_buf, 1, NULL);
            }
        } else {
            // Minimal sleep to avoid pegging CPU if no interrupts used
            vTaskDelay(1);
        }
    }
}
