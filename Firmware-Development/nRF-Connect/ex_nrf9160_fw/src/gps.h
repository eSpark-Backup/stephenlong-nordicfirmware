extern struct k_sem pvt_data_sem;
extern struct nrf_modem_gnss_pvt_data_frame pvt_data;
void gnss_event_handler(int event);
void gnss_init();
int gnss_config(int gps_fix_retry_interval);