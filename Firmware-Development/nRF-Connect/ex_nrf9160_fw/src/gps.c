#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <modem/lte_lc.h>
#include <nrf_modem_gnss.h>
#include <modem/nrf_modem_lib.h>
#include <modem/modem_info.h>

#include "gps.h"
#include "main.h"

#define GNSS_FIX_RETRY_SEC 90

K_SEM_DEFINE(pvt_data_sem, 0, 1);

struct nrf_modem_gnss_pvt_data_frame pvt_data;

void gnss_event_handler(int event)
{
	switch (event) {
		
		case NRF_MODEM_GNSS_EVT_PVT:
			//printk("NRF_MODEM_GNSS_EVT_PVT:\n");
			if (nrf_modem_gnss_read(&pvt_data, sizeof(pvt_data), NRF_MODEM_GNSS_DATA_PVT) != 0) {
				printk("Error reading PVT data\n");
			} else {
				k_work_schedule(&gnss_work, K_NO_WAIT);
				//k_sem_give(&pvt_data_sem);
			}
			break;
		case NRF_MODEM_GNSS_EVT_FIX:
			printk("NRF_MODEM_GNSS_EVT_FIX:\n");
			break;
		case NRF_MODEM_GNSS_EVT_NMEA:
			printk("NRF_MODEM_GNSS_EVT_NMEA:\n");
			break;
		case NRF_MODEM_GNSS_EVT_AGNSS_REQ:
			printk("NRF_MODEM_GNSS_EVT_AGNSS_REQ:\n");
			break;
		case NRF_MODEM_GNSS_EVT_BLOCKED:
			printk("NRF_MODEM_GNSS_EVT_BLOCKED:\n");
			break;
		case NRF_MODEM_GNSS_EVT_UNBLOCKED:
			printk("NRF_MODEM_GNSS_EVT_UNBLOCKED:\n");
			break;
		case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
			printk("NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:\n");
			break;
		case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
			printk("NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:\n");
			break;
		case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
			printk("NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:\n");
			break;
		case NRF_MODEM_GNSS_EVT_REF_ALT_EXPIRED:
			printk("NRF_MODEM_GNSS_EVT_REF_ALT_EXPIRED:\n");
			break;

		default:
			printk("Unknown GNSS Event %d\n",event);
			break;
	}
}

int gnss_config(int gps_fix_retry_interval){

	int err;

	if ((err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS))) {
		printk("Failed enabling GNSS Functional Mode, error: %d\n", err);
	}

	// Set to 0 for single fix mode. GNSS will switch off after valid
	// fix or when fix_retry_set has been reached.
	// Set to 1 for continuous navigation. GNSS receiver to produce
	// PVT @ 1Hz
	err = nrf_modem_gnss_fix_interval_set(0);
	// Number of times to try for a fix
	// nrf_modem_gnss_fix_retry_set(GNSS_FIX_RETRY_SEC);
	// For now set to maximum value which is the gps interval, shall be finalized later
	err = nrf_modem_gnss_fix_retry_set(gps_fix_retry_interval - 10);

	// By default, all NMEA strings are disabled.
#ifdef NMEA
	/* Enable all supported NMEA messages. */
	uint16_t nmea_mask = 	NRF_MODEM_GNSS_NMEA_RMC_MASK |
							NRF_MODEM_GNSS_NMEA_GGA_MASK |
							NRF_MODEM_GNSS_NMEA_GLL_MASK |
							NRF_MODEM_GNSS_NMEA_GSA_MASK |
							NRF_MODEM_GNSS_NMEA_GSV_MASK;

	if (nrf_modem_gnss_nmea_mask_set(nmea_mask) != 0) {
		printk("Failed to set GNSS NMEA mask\n");
	}
#endif

	// Enable low accuracy mode
	if ((err = nrf_modem_gnss_use_case_set(NRF_MODEM_GNSS_USE_CASE_MULTIPLE_HOT_START | NRF_MODEM_GNSS_USE_CASE_LOW_ACCURACY))) {
		printk("Failed to set low accuracy mode, error %d\n",err);
	}

	return err;
}

void gnss_init(){

	int err;

	if ((err = nrf_modem_gnss_event_handler_set(gnss_event_handler))) {
		printk("Failed to set GNSS event handler, error: %d\n", err);
	}

	err = gnss_config(gps_interval);
	if (err != 0) {
		printk("Error during GNSS configuration: %d\n", err);
	}

	k_work_schedule(&gnss_start_single_fix, K_SECONDS(gps_interval));

}