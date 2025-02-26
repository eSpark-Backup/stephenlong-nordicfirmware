
extern struct k_work_delayable shadow_update_work;
extern struct k_work_delayable connect_work;
extern struct k_work shadow_update_version_work;
extern struct k_work_delayable gnss_start_single_fix;
extern struct k_work_delayable gnss_work;

extern bool cloud_connected;
extern bool gps_fix_valid;
extern int gps_interval;
extern int aws_interval;
extern double last_reported_latitude;
extern double last_reported_longitude;

void connect_work_fn(struct k_work *work);
void shadow_update_work_fn(struct k_work *work);
void shadow_update_version_work_fn(struct k_work *work);
void gnss_start_single_fix_fn(struct k_work *work);
void gnss_work_fn();

