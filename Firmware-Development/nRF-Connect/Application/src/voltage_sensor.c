#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "voltage_sensor.h"

#define ADC_NODE DT_ALIAS(adc0)
#define ADC_RESOLUTION 12
/* Voltage Divider Resistors */
#define R1 100000 /* 100kΩ */
#define R2 6000   /* 6kΩ */

/* Divider Gain Calculation */
#define VOLTAGE_DIVIDER_RATIO ((R1 + R2) / (float)R2)  /* 17.67 */
/* ADC Device */
static const struct device *adc = DEVICE_DT_GET(ADC_NODE);

/* ADC channel configuration for Channel 0 */
static const struct adc_channel_cfg channel_cfg = {
    .gain = ADC_GAIN_1_6,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = 0,
    .input_positive = 2, // P0.03
};

/* ADC buffer */
static uint16_t adc_sample;
static uint32_t vref_mv = 0;

/* ADC sequence configuration */
static struct adc_sequence sequence = {
    .channels = BIT(0),
    .buffer = &adc_sample,
    .buffer_size = sizeof(adc_sample),
    .resolution = ADC_RESOLUTION,
};

/* Initialize ADC */
int voltage_sensor_init(void)
{
    if (!device_is_ready(adc)) {
        printk("ADC device not ready\n");
        return -1;
    }

    int err = adc_channel_setup(adc, &channel_cfg);
    if (err) {
        printk("Failed to configure ADC channel (err: %d)\n", err);
        return err;
    }

    if (channel_cfg.reference == ADC_REF_INTERNAL) {
        vref_mv = adc_ref_internal(adc);
    }

    return 0;
}

int voltage_sensor_read(float *voltage)
{
    int err = adc_read(adc, &sequence);
    if (err < 0) {
        printk("ADC read failed (err: %d)\n", err);
        return err;
    }

    int32_t value_mv = adc_sample;
    
    err = adc_raw_to_millivolts(vref_mv, channel_cfg.gain, ADC_RESOLUTION, &value_mv);
    if (err < 0) {
        printf(" (value in mV not available)\n");
        return err;
    }

    *voltage = (value_mv * VOLTAGE_DIVIDER_RATIO) / 1000.f;
    return 0;
}
