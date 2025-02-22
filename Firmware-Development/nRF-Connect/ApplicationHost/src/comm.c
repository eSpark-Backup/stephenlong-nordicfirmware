#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <string.h>
#include "comm.h"

// #define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));

void comm_print(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
    uart_poll_out(uart_dev, '\r');
    uart_poll_out(uart_dev, '\n');
}

int comm_init(void)
{
	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
        return -1;
	}

    return 0;
}