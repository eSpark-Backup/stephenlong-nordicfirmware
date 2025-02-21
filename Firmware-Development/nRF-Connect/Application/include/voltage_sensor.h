#pragma once


/* Initialize ADC */
int voltage_sensor_init(void);

/* Read ADC channel 0 */
int voltage_sensor_read(float *voltage);
