#include "adc.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

const static char *TAG = "ADC";

#define ADC_ATTEN ADC_ATTEN_DB_11

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t chanel,
                                 adc_atten_t atten,
                                 adc_cali_handle_t *out_handle);
static void adc_calibration_deinit(adc_cali_handle_t handle);

adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_handle = NULL;
bool do_calibration;

void adc1_init(adc_bitwidth_t bitwidth, adc_channel_t *channels,
               size_t channel_size)
{
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = bitwidth,
        .atten = ADC_ATTEN,
    };
    for (int i = 0; i < channel_size; i++)
    {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, channels[i],
                                                   &chan_cfg));

        //-------------ADC1 Calibration Init---------------//
        do_calibration =
            adc_calibration_init(ADC_UNIT_1, channels[i], ADC_ATTEN,
                                 &adc1_cali_handle);
    }
}

int adc1_get_raw(adc_channel_t channel)
{
    int value;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, &value));
    return value;
}

int adc1_read(adc_channel_t channel, size_t num_of_samples)
{
    int raw;
    int raw_sum = 0;
    int voltage = 0;
    for (int i = 0; i < num_of_samples; i++)
    {
        raw = adc1_get_raw(channel);
        raw_sum += raw;
    }
    raw = raw_sum / num_of_samples;
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, channel, raw);
    if (do_calibration)
    {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1,
                 channel, voltage);
    }
    return voltage;
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel,
                                 adc_atten_t atten,
                                 adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}
