/*
 * Copyright (c) 2022 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <cJSON.h>
#include <string.h>

LOG_MODULE_REGISTER(uart_logger, LOG_LEVEL_INF);

/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

#define MSG_SIZE 128
#define UART_TIMEOUT_MS 1000

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;
static struct k_timer uart_timer;

void send_buffer_to_queue(void)
{
    if (rx_buf_pos > 0)
    {
        rx_buf[rx_buf_pos] = '\0';
        k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);
        rx_buf_pos = 0;
    }
}

void uart_timeout_handler(struct k_timer *timer_id)
{
    send_buffer_to_queue();
}

void serial_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    if (!uart_irq_update(uart_dev))
    {
        return;
    }

    if (!uart_irq_rx_ready(uart_dev))
    {
        return;
    }

    while (uart_fifo_read(uart_dev, &c, 1) == 1)
    {
        if (rx_buf_pos < (sizeof(rx_buf) - 1))
        {
            rx_buf[rx_buf_pos++] = c;
            k_timer_start(&uart_timer, K_MSEC(UART_TIMEOUT_MS), K_NO_WAIT);
        }
        else
        {
            printk("RX buffer overflow, resetting\n");
            rx_buf_pos = 0;
        }
    }
}

void print_uart(char *buf)
{
    int msg_len = strlen(buf);
    for (int i = 0; i < msg_len; i++)
    {
        uart_poll_out(uart_dev, buf[i]);
    }
}

int main(void)
{
    char tx_buf[MSG_SIZE];

    if (!device_is_ready(uart_dev))
    {
        printk("UART device not found!\n");
        return 0;
    }

    int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

    if (ret < 0)
    {
        printk("Error setting UART callback: %d\n", ret);
        return 0;
    }

    uart_irq_rx_enable(uart_dev);
    k_timer_init(&uart_timer, uart_timeout_handler, NULL);

    LOG_INF("nrf9160 Host Board Application Starting ");
    while (k_msgq_get(&uart_msgq, &tx_buf, K_FOREVER) == 0)
    {
    //     print_uart("Received : ");
    //     print_uart(tx_buf);
    //     print_uart("\r\n");
        LOG_INF("Received message: %s", tx_buf);
    }
    return 0;
}
