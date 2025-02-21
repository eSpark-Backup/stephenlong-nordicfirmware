/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/device.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/sensor_types.h>
#include <zephyr/random/random.h>
#include "model_handler.h"
#include "dht11.h"

float temperature, humidity;
#define TEMP_HUMIDITY_SEND_INTERVAL K_SECONDS(5) // Send data every 10 seconds

static struct k_work_delayable temp_humidity_work;

static int temp_get(struct bt_mesh_sensor_srv *srv,
                    struct bt_mesh_sensor *sensor,
                    struct bt_mesh_msg_ctx *ctx,
                    struct bt_mesh_sensor_value *rsp)
{
    // float dummy_temp = 25.0 + (sys_rand32_get() % 1000) / 100.0; // Random temp between 25.0 - 35.0°C
    int err = bt_mesh_sensor_value_from_float(sensor->type->channels[0].format, temperature, rsp);

    if (err && err != -ERANGE) {
		printk("Error encoding temperature (%d)", err);
		return err;
	}

    printk("Getting Temperature: %.2f°C\n", temperature);
    return 0;
}

static int humidity_get(struct bt_mesh_sensor_srv *srv,
                        struct bt_mesh_sensor *sensor,
                        struct bt_mesh_msg_ctx *ctx,
                        struct bt_mesh_sensor_value *rsp)
{
    // float dummy_humid = 50.0 + (sys_rand32_get() % 500) / 10.0; // Random humidity between 50% - 100%
    int err = bt_mesh_sensor_value_from_float(sensor->type->channels[0].format, humidity, rsp);

    if (err && err != -ERANGE) {
		printk("Error encoding temperature (%d)", err);
		return err;
	}

    printk("Getting Humidity: %.2f%%\n", humidity);
    return 0;
}

static struct bt_mesh_sensor temperature_sensor = {
    .type = &bt_mesh_sensor_precise_present_amb_temp,
    .get = temp_get,
};

static struct bt_mesh_sensor humidity_sensor = {
    .type = &bt_mesh_sensor_present_amb_rel_humidity,
    .get = humidity_get,
};

static struct bt_mesh_sensor *const temperature_sensor_data[] = {
    &temperature_sensor
};

static struct bt_mesh_sensor *const humidity_sensor_data[] = {
    &humidity_sensor,
};

static struct bt_mesh_sensor_srv temperature_sensor_srv =
    BT_MESH_SENSOR_SRV_INIT(temperature_sensor_data, ARRAY_SIZE(temperature_sensor_data));
static struct bt_mesh_sensor_srv humidity_sensor_srv =
	BT_MESH_SENSOR_SRV_INIT(humidity_sensor_data, ARRAY_SIZE(humidity_sensor_data));

static struct bt_mesh_elem elements[] = {
    BT_MESH_ELEM(1,
                 BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV,
                                    BT_MESH_MODEL_SENSOR_SRV(&temperature_sensor_srv)),
                 BT_MESH_MODEL_NONE),
    BT_MESH_ELEM(2,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_SENSOR_SRV(&humidity_sensor_srv)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
    .cid = CONFIG_BT_COMPANY_ID,
    .elem = elements,
    .elem_count = ARRAY_SIZE(elements),
};

static void temp_humidity_send(struct k_work *work)
{
    struct bt_mesh_sensor_value temp_value, humid_value;

    dht_sensor_read(&temperature, &humidity);

    temp_get(NULL, &temperature_sensor, NULL, &temp_value);
    humidity_get(NULL, &humidity_sensor, NULL, &humid_value);

    printk("Sending Temperature and Humidity\n");
    
    bt_mesh_sensor_srv_pub(&temperature_sensor_srv, NULL, &temperature_sensor, &temp_value);
    bt_mesh_sensor_srv_pub(&humidity_sensor_srv, NULL, &humidity_sensor, &humid_value);
    
    k_work_reschedule(&temp_humidity_work, TEMP_HUMIDITY_SEND_INTERVAL);
}

const struct bt_mesh_comp *model_handler_init(void)
{
    dht_sensor_init();

    k_work_init_delayable(&temp_humidity_work, temp_humidity_send);
    k_work_reschedule(&temp_humidity_work, TEMP_HUMIDITY_SEND_INTERVAL);
    return &comp;
}
