/*
 * Copyright (c) 2023 Ian Morris
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor_data_types.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/dsp/print_format.h>

#define DHT_ALIAS DT_ALIAS(dht0)

static const struct device *const sensor = DEVICE_DT_GET(DHT_ALIAS);

SENSOR_DT_READ_IODEV(dht_iodev, DHT_ALIAS,
                     {SENSOR_CHAN_AMBIENT_TEMP, 0},
                     {SENSOR_CHAN_HUMIDITY, 0});

static struct rtio_iodev *dht_iodev_ptr = &dht_iodev;

RTIO_DEFINE(dht_ctx, 1, 1);

float q31_to_float(q31_t q, int shift) {
    char str_value[20]; // Buffer to store the formatted string
    snprintf(str_value, sizeof(str_value), "%s%d.%d", PRIq_arg(q, 2, shift));

    return strtof(str_value, NULL); // Convert string to float
}

/**
 * @brief Initialize the DHT sensor.
 * @return 0 on success, or a negative error code on failure.
 */
int dht_sensor_init(void)
{
    if (!device_is_ready(sensor))
    {
        printk("sensor: device %s not ready.\n", sensor->name);
        return -ENODEV;
    }
    return 0;
}

/**
 * @brief Read temperature and humidity data from the DHT sensor.
 * @param temp Pointer to store the temperature value.
 * @param hum Pointer to store the humidity value.
 * @return 0 on success, or a negative error code on failure.
 */
int dht_sensor_read(float *temp, float *hum)
{
    int rc;
    uint8_t buf[128];

    rc = sensor_read(dht_iodev_ptr, &dht_ctx, buf, 128);
    if (rc != 0)
    {
        printk("%s: sensor_read() failed: %d\n", sensor->name, rc);
        return rc;
    }

    const struct sensor_decoder_api *decoder;
    rc = sensor_get_decoder(sensor, &decoder);
    if (rc != 0)
    {
        printk("%s: sensor_get_decode() failed: %d\n", sensor->name, rc);
        return rc;
    }

    uint32_t temp_fit = 0;
    struct sensor_q31_data temp_data = {0};
    decoder->decode(buf,
                    (struct sensor_chan_spec){SENSOR_CHAN_AMBIENT_TEMP, 0},
                    &temp_fit, 1, &temp_data);

    uint32_t hum_fit = 0;
    struct sensor_q31_data hum_data = {0};
    decoder->decode(buf,
                    (struct sensor_chan_spec){SENSOR_CHAN_HUMIDITY, 0},
                    &hum_fit, 1, &hum_data);

    // printk("temp is %s%d.%d °C humidity is %s%d.%d %%RH\n", PRIq_arg(temp_data.readings[0].temperature, 2, temp_data.shift),
    //     PRIq_arg(hum_data.readings[0].humidity, 2, hum_data.shift));

    // Convert Q31 format to float
    *temp = q31_to_float(temp_data.readings[0].temperature, temp_data.shift);
    *hum = q31_to_float(hum_data.readings[0].humidity, hum_data.shift);

    return 0;
}
