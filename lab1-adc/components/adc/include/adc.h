#ifndef ADC_H
#define ADC_H

#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"
#include <stdio.h>

/**
 * @brief Initialize ADC1 with the specified bitwidth or resolution. ESP32-C6
 * only supports up to ADC_BITWIDTH_12 or 12 bits or ADC resolution.
 *
 * @param bitwidth Select the resolution of ADC1
 * @param channels Array of ADC channels to be configured
 * @param channel_size Size of the array
 */
void adc1_init(adc_bitwidth_t bitwidth, adc_channel_t *channels,
               size_t channel_size);

/**
 * @brief Get raw value from ADC, ranging from 0 - (2^adc_bithwidth)
 *
 * @param channel ADC channel to read from
 * @return int
 */
int adc1_get_raw(adc_channel_t channel);

/**
 * @brief Read ADC from a specified channel for a specified number of samples,
 * Then return the average value in millivolts
 *
 * @param channel ADC channel to read from
 * @param num_of_samples Number of samples to read
 * @return int
 */
int adc1_read(adc_channel_t channel, size_t num_of_samples);

#endif // ADC_H
