/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <stdlib.h>
#include <nrf_modem_gnss.h>
#if defined(CONFIG_NRF_MODEM_LIB)
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <modem/modem_info.h>
#include <nrf_modem.h>
#endif
#include <net/aws_iot.h>
#include <zephyr/sys/reboot.h>
#include <date_time.h>
#include <zephyr/dfu/mcuboot.h>
#include <cJSON.h>
#include <cJSON_os.h>
#include <hw_id.h>

#include "datetime.h"
#include "lte.h"
#include "dfu.h"
#include "gps.h"
#include "aws.h"
#include "json_support.h"
#include "main.h"

#define DEBUG_ON
#define SEND_LAST_VALID_FIX_TO_CLOUD
#define DEFAULT_GPS_FIX_INTERVAL 120
#define DEFAULT_AWS_INTERVAL 300

BUILD_ASSERT(!IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT),
		"This sample does not support LTE auto-init and connect");

struct k_work_delayable shadow_update_work;
struct k_work_delayable connect_work;
struct k_work shadow_update_version_work;
struct k_work_delayable gnss_start_single_fix;
struct k_work_delayable gnss_work;

bool cloud_connected;
bool gps_fix_valid = false;
int gps_interval = DEFAULT_GPS_FIX_INTERVAL;
int aws_interval = DEFAULT_AWS_INTERVAL;
double last_reported_latitude;
double last_reported_longitude;
static bool gnss_is_running = false;
static int gnss_not_enough_window_time_cnt = 0;
static int last_set_gps_interval = DEFAULT_GPS_FIX_INTERVAL;

static char hw_id[HW_ID_LEN];

void pvtflagsstatus(){
	if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_DEADLINE_MISSED) {
		printk("GNSS blocked by LTE activity\n");
	}
	if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_LEAP_SECOND_VALID) {
		printk("The leap second information is valid\n");
	}
	if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_SLEEP_BETWEEN_PVT) {
		printk("At least one sleep period has passed since the last PVT notification\n");
	}
	if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_VELOCITY_VALID) {
		printk("The velocity estimate is valid\n");
	} 
	if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_SCHED_DOWNLOAD) {
		printk("The PVT notification was sent because of a scheduled download\n");
	}
}

int main(void)
{
	int err;

	printk("The AWS IoT sample started, version: %s\n", CONFIG_APP_VERSION);

	cJSON_Init();

	// This example supports Amazon Web Services firmware over-the-air (AWS FOTA)
	// Update. Check update status, if FW update has just occured.
	nrf_modem_lib_dfu_handler();

	// This thread must be initialized before gnss_init() as this func schedules it
	k_work_init_delayable(&gnss_start_single_fix, gnss_start_single_fix_fn);
	// Initialize GNSS in Single fix mode
	gnss_init();

	// Initialise modem information module. Used to request battery voltage data
	// from the modem
	err = modem_info_init();
	if (err) {
		printk("Failed initializing modem info module, error: %d\n", err);
	}

	// Obtain the modems' IMEI (International Mobile Equipment Identity). We use
	// this as the client ID (thing ID) when communicating with the AWS IoT Broker
	err = hw_id_get(hw_id, ARRAY_SIZE(hw_id));
	if (err) {
		printk("Failed to retrieve hardware ID, error: %d", err);
		return err;
	}

	printk("IMEI: %s\n", hw_id);

	// The AWS IOT Client ID should match the 'Thing Name' on the 
	// AWS IoT console. It can be set in the prj.conf using: 
	// CONFIG_AWS_IOT_CLIENT_ID_STATIC="<Thing Name>" 
	// or at run time using the aws_iot_config structure passed 
	// to aws_iot_connect(); If using this option, set 
	// CONFIG_AWS_IOT_CLIENT_ID_APP=y
	
	// Setting the Client ID at runtime allows the developer to 
	// easly deploy fleets of devices with the same firmware. 
	// In this case we read the modem's IMEI and use this as
	// the Client ID.

	// Initialise Amazon Web Services IoT Module
	err = aws_iot_init(aws_iot_event_handler);
	if (err) {
		printk("AWS IoT library could not be initialized, error: %d\n", err);
	}

	// Subscribe to customizable non-shadow specific topics.
	// Topics will be subscribed to upon connection to AWS IoT broker.
	err = app_topics_subscribe();
	if (err) {
		printk("Adding application specific topics failed, error: %d\n", err);
	}

	// Initialise Workqueue Threads 
	// Thread connect_work is responsible for connecting and reconnecting to the 
	// AWS IoT broker.
	k_work_init_delayable(&connect_work, connect_work_fn);
	// Thread shadow_update_version_work only publishes a message (with version 
	// number) upon an initial connection. 
	k_work_init(&shadow_update_version_work, shadow_update_version_work_fn);
	// Thread shadow_update_work continues to periodically publish a message 
	// (without version number)
	k_work_init_delayable(&shadow_update_work, shadow_update_work_fn);
	// Thread gnss_start_single_fix periodically takes a single fix mode gps fix, it must be initialized before gnss_init() as the func schedules it
	// k_work_init_delayable(&gnss_start_single_fix, gnss_start_single_fix_fn);
	// Thread gnss_work checks for valid gps fixes and processes it
	k_work_init_delayable(&gnss_work, gnss_work_fn);
		
	// Initialise LTE modem and LTE handler callback
	modem_configure();

	// Wait for LTE modem to connect 
	k_sem_take(&lte_connected, K_FOREVER);

	// Update date & time
	date_time_update_async(date_time_event_handler);

	// Initiate connection to AWS IoT MQTT 
	k_work_schedule(&connect_work, K_NO_WAIT);

}

void connect_work_fn(struct k_work *work)
{
	int err;

	if (cloud_connected) {
		return;
	}

	const struct aws_iot_config config = {
		.client_id = hw_id,
	};

	// Connect to AWS IoT broker (MQTT)
	err = aws_iot_connect(&config);
	if (err) {
		printk("aws_iot_connect, error: %d\n", err);
		if(err == -EOPNOTSUPP || err == -ENOTCONN){

		}
	}

	printk("Next connection retry in %d seconds\n", CONFIG_CONNECTION_RETRY_TIMEOUT_SECONDS);

	k_work_schedule(&connect_work, K_SECONDS(CONFIG_CONNECTION_RETRY_TIMEOUT_SECONDS));
}

void shadow_update_work_fn(struct k_work *work)
{
	int err;

	if (!cloud_connected) {
		return;
	}

	// Update shadow with-out version number (Type 1)
	err = shadow_update(false);
	if (err) {
		printk("shadow_update, error: %d\n", err);
	}

#ifdef SEND_LAST_VALID_FIX_TO_CLOUD
	gps_fix_valid = false;
#endif

	printk("Next data publication in %d seconds\n", aws_interval);

	k_work_schedule(&shadow_update_work, K_SECONDS(aws_interval));
}

void shadow_update_version_work_fn(struct k_work *work)
{
	int err;

	// Update shadow with version number (Type 2)
	err = shadow_update(true);
	if (err) {
		printk("shadow_update, error: %d\n", err);
	}
}

void gnss_start_single_fix_fn(struct k_work *work)
{
	int err;
#ifndef SEND_LAST_VALID_FIX_TO_CLOUD
	gps_fix_valid = false;
#endif

	// Stop GNSS if it is already running
	if (gnss_is_running) {
		printk("Stopping GNSS\n");
		err = nrf_modem_gnss_stop();
		if (err) {
			printk("Failed to stop GNSS, error %d\n", err);
			return; // Exit early on failure to avoid conflicts
		}
		gnss_is_running = false; // GNSS is now stopped
	}

	// Reconfigure GNSS
	if(gps_interval != last_set_gps_interval){
		last_set_gps_interval = gps_interval;
		printk("Reconfiguring GNSS Fix Retry Interval\n");
		err = nrf_modem_gnss_fix_retry_set(gps_interval - 10);
		if (err) {
			printk("Error during GNSS re-configuration: %d\n", err);
			return; // Exit early on failure
		}
	}
	
	// Start GNSS
	printk("Starting GNSS\n");
    err = nrf_modem_gnss_start();
    if (err) {
        printk("Failed to start GNSS, error %d\n", err);
        return; // Exit early on failure
    }
	gnss_is_running = true;

	// //Must be called when GNSS is running
	// printk("Enabling Priority\n");
	// err = nrf_modem_gnss_prio_mode_enable();
	// if (err) {
	// 	printk("Could not set priority mode for GNSS (%d)", err);
	// }

	printk("Next fix to be taken in %d seconds\n", gps_interval);
	k_work_schedule(&gnss_start_single_fix, K_SECONDS(gps_interval));
}

void gnss_work_fn(){

#ifdef DEBUG_ON
	pvtflagsstatus();
#endif

	if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_NOT_ENOUGH_WINDOW_TIME) {
		printk("Average time windows granted to GNSS may be too short\n");
		//GNSS priority may interfere with LTE operations, In general, long enough
		//eDRX cycles, or PSM, will ensure better functionality for both GNSS and LTE.
		//Mess with priority only when PSM is refused by network provider
		if (psm_activated == false){
			gnss_not_enough_window_time_cnt++;
			if (gnss_not_enough_window_time_cnt == 3){
				//Must be called when GNSS is running
				printk("Enabling GNSS Priority Over RCC IDLE\n");
				if (nrf_modem_gnss_prio_mode_enable()) {
					printk("Could not set priority mode for GNSS");
				}
				gnss_not_enough_window_time_cnt = 0;
			}
		}
	}

	if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
		printk("Latitude: %.06f Longitude: %.06f\n", pvt_data.latitude, pvt_data.longitude);
		last_reported_latitude = pvt_data.latitude;
		last_reported_longitude = pvt_data.longitude;
		gps_fix_valid = true;
	}
	else {
		int found = 0;
		//int fix = 0;
		for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i) {
			if (pvt_data.sv[i].sv > 0) {
				found++;
				//Print satelite number:
				//printf("SV Number %d ", pvt_data.sv[i].sv);

				//Flag is only set once we have a fix, and at that
				//stage, this routine is no longer called.
				//if (pvt_data.sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX) fix++;
			}
		}
		printk("Searching: Found %d SV\n",found);
	}
}