#include "current_sensor.h"


static const char *TAG = "CURRENT_SENSOR";

// INA219 configuration constants
#define INA219_CONFIG_RESET     0x8000
#define INA219_CONFIG_BVOLTAGERANGE_32V 0x2000
#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS 0x0007

static float round_up_lsb(float lsb, float step) {
    if (lsb <= 0.0f) return step;
    int n = (int)ceilf(lsb / step);
    return n * step;
}

static slave_pcb_err_t ina219_calibrate(uint8_t channel, float shunt_resistor_ohms, float max_expected_current_a) {
    if (shunt_resistor_ohms <= 0.0f || max_expected_current_a <= 0.0f) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Reset
    slave_pcb_err_t ret = i2c_write_register(CURRENT_SENSOR_ADDR, INA219_REG_CONFIG, INA219_CONFIG_RESET);
    if (ret != SLAVE_PCB_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));

    // Config: 32V bus range + PGA = ±80mV (0x0800) + continuous
    uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                      0x0800 |  // PGA = ±80 mV
                      INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
    ret = i2c_write_register(CURRENT_SENSOR_ADDR, INA219_REG_CONFIG, config);
    if (ret != SLAVE_PCB_OK) return ret;

    // Current LSB (datasheet): MaxExpected / 32768
    float current_lsb = max_expected_current_a / 32768.0f;

    // Arrondir vers le haut à un pas pratique (ici 20 µA)
    const float LSB_STEP = 20e-6f; // 20 µA
    current_lsb = round_up_lsb(current_lsb, LSB_STEP);

    // CAL
    float cal_f = 0.04096f / (current_lsb * shunt_resistor_ohms);
    uint32_t cal_u32 = (uint32_t)floorf(cal_f);
    if (cal_u32 < 1) cal_u32 = 1;
    if (cal_u32 > 0xFFFE) cal_u32 = 0xFFFE;
    uint16_t cal = (uint16_t)cal_u32;

    ret = i2c_write_register(CURRENT_SENSOR_ADDR, INA219_REG_CALIBRATION, cal);
    return ret;
}

slave_pcb_err_t current_sensor_init(void) {
    ESP_LOGI(TAG, "Current sensor manager initialized");
    return SLAVE_PCB_OK;
}

float get_shunt_resistor_for_channel(uint8_t channel) {
    if (channel < 6) {
        return SHUNT_RESISTOR_ELECTROVALVE;
    } else if (channel == 6 || channel == 7) {
        return SHUNT_RESISTOR_PUMP_PE_PV;
    } else {
        return SHUNT_RESISTOR_PUMP_OTHER;
    }
}

float get_max_current_for_channel(uint8_t channel) {
    if (channel < 6) {
        return MAX_CURRENT_ELECTROVALVE_AMP;
    } else if (channel == 6) {
        return MAX_CURRENT_PUMP_PE_AMP;
    } else if (channel == 7) {
        return MAX_CURRENT_PUMP_PV_AMP;
    } else {
        return 0.0f;
    }
}

slave_pcb_err_t current_sensor_read_channel(uint8_t channel, float *current_ma) {
    if (!current_ma) {
        REPORT_ERROR(SLAVE_PCB_ERR_INVALID_ARG, TAG, "Null pointer for current reading", channel);
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    float shunt_resistor = get_shunt_resistor_for_channel(channel);
    if (shunt_resistor <= 0.0f) {
        *current_ma = 0.0f;
        return SLAVE_PCB_OK;
    }

    // Set multiplexer channel
    slave_pcb_err_t ret = i2c_set_multiplexer_channel(channel);
    if (ret != SLAVE_PCB_OK) return ret;

    // Calibrate sensor
    ret = ina219_calibrate(channel, shunt_resistor,get_max_current_for_channel(channel));
    if (ret != SLAVE_PCB_OK) {
        *current_ma = 0.0f;
        return ret;
    }

    // Read current
    uint16_t raw_current;
    ret = i2c_read_register(CURRENT_SENSOR_ADDR, INA219_REG_CURRENT, &raw_current);
    if (ret != SLAVE_PCB_OK) {
        *current_ma = 0.0f;
        return ret;
    }

    // Convert to mA
    int16_t signed_current = (int16_t)raw_current;
    float max_current_a = get_max_current_for_channel(channel);
    float current_lsb = max_current_a / 32768.0f;
    *current_ma = (float)signed_current * current_lsb * 1000.0f;

    ESP_LOGD(TAG, "Channel %d: %.1f mA", channel, *current_ma);
    return SLAVE_PCB_OK;
}

