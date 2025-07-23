#include "slave_pcb.h"

static const char *TAG = "COMM_MGR";

// SPI device handle for W5500
static spi_device_handle_t w5500_spi;

// Ethernet configuration
static const uint8_t mac_addr[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
static const uint8_t ip_addr[4] = {192, 168, 1, 100};
static const uint8_t subnet_mask[4] = {255, 255, 255, 0};
static const uint8_t gateway[4] = {192, 168, 1, 1};

/**
 * @brief Initialize SPI for W5500 Ethernet controller
 */
static slave_pcb_err_t init_w5500_spi(void) {
    ESP_LOGI(TAG, "Initializing SPI for W5500");

    // SPI bus configuration
    spi_bus_config_t bus_config = {
        .mosi_io_num = SPI_MOSI,
        .miso_io_num = SPI_MISO,
        .sclk_io_num = SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return SLAVE_PCB_ERR_SPI_FAIL;
    }

    // SPI device configuration for W5500
    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
        .mode = 0,
        .spics_io_num = SPI_CS,
        .queue_size = 20,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    ret = spi_bus_add_device(SPI2_HOST, &dev_config, &w5500_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return SLAVE_PCB_ERR_SPI_FAIL;
    }

    // Reset W5500
    gpio_set_level(W5500_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(W5500_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "W5500 SPI initialization completed");
    return SLAVE_PCB_OK;
}

/**
 * @brief Read register from W5500
 */
static esp_err_t w5500_read_reg(uint16_t addr, uint8_t *data, size_t len) {
    spi_transaction_t trans = {
        .length = 24, // 3 bytes command
        .tx_data = {(addr >> 8) & 0xFF, addr & 0xFF, 0x00}, // Address + control byte for read
        .flags = SPI_TRANS_USE_TXDATA,
    };

    esp_err_t ret = spi_device_transmit(w5500_spi, &trans);
    if (ret != ESP_OK) {
        return ret;
    }

    // Read data
    spi_transaction_t read_trans = {
        .length = len * 8,
        .rx_buffer = data,
    };

    return spi_device_transmit(w5500_spi, &read_trans);
}

/**
 * @brief Write register to W5500
 */
static esp_err_t w5500_write_reg(uint16_t addr, const uint8_t *data, size_t len) {
    uint8_t tx_buf[256];
    if (len > 253) {
        return ESP_ERR_INVALID_SIZE;
    }

    tx_buf[0] = (addr >> 8) & 0xFF;
    tx_buf[1] = addr & 0xFF;
    tx_buf[2] = 0x04; // Control byte for write
    memcpy(&tx_buf[3], data, len);

    spi_transaction_t trans = {
        .length = (3 + len) * 8,
        .tx_buffer = tx_buf,
    };

    return spi_device_transmit(w5500_spi, &trans);
}

/**
 * @brief Initialize W5500 with network configuration
 */
static slave_pcb_err_t init_w5500_network(void) {
    ESP_LOGI(TAG, "Initializing W5500 network configuration");

    // Set MAC address
    esp_err_t ret = w5500_write_reg(0x0009, mac_addr, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set MAC address");
        return SLAVE_PCB_ERR_SPI_FAIL;
    }

    // Set IP address
    ret = w5500_write_reg(0x000F, ip_addr, 4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set IP address");
        return SLAVE_PCB_ERR_SPI_FAIL;
    }

    // Set subnet mask
    ret = w5500_write_reg(0x0005, subnet_mask, 4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set subnet mask");
        return SLAVE_PCB_ERR_SPI_FAIL;
    }

    // Set gateway
    ret = w5500_write_reg(0x0001, gateway, 4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set gateway");
        return SLAVE_PCB_ERR_SPI_FAIL;
    }

    ESP_LOGI(TAG, "W5500 network configuration completed");
    ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    ESP_LOGI(TAG, "IP: %d.%d.%d.%d", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
    
    return SLAVE_PCB_OK;
}

/**
 * @brief Send message to MainPCB via Ethernet
 */
static slave_pcb_err_t send_message_to_main_pcb(const comm_msg_t *msg) {
    if (!msg) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Sending message type %d to MainPCB", msg->type);

    // TODO: Implement actual TCP/UDP communication with MainPCB
    // For now, just log the message
    switch (msg->type) {
        case MSG_CASE_CHANGE:
            ESP_LOGI(TAG, "Sending case change: %s", get_case_string(msg->data.case_data));
            break;
        case MSG_BUTTON_STATE:
            ESP_LOGI(TAG, "Sending button state: Button %d = %s", 
                     msg->data.button_data.button, 
                     msg->data.button_data.state ? "PRESSED" : "RELEASED");
            break;
        case MSG_LOAD_CELL_DATA:
            ESP_LOGI(TAG, "Sending load cell data: Tank %d = %.2f kg", 
                     msg->data.load_cell_data.tank, 
                     msg->data.load_cell_data.weight);
            break;
        case MSG_DEVICE_STATUS:
            ESP_LOGI(TAG, "Sending device status: Device %d = %s", 
                     msg->data.device_status.device, 
                     msg->data.device_status.status ? "OK" : "FAULT");
            break;
        case MSG_ERROR:
            ESP_LOGI(TAG, "Sending error: %s", msg->data.error_data.description);
            break;
        default:
            ESP_LOGW(TAG, "Unknown message type: %d", msg->type);
            break;
    }

    return SLAVE_PCB_OK;
}

/**
 * @brief Receive and process messages from MainPCB
 */
static slave_pcb_err_t receive_message_from_main_pcb(void) {
    // TODO: Implement actual TCP/UDP reception from MainPCB
    // For now, simulate occasional messages for testing
    
    static uint32_t last_sim_msg = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (now - last_sim_msg > 30000) { // Every 30 seconds, simulate a message
        last_sim_msg = now;
        
        // Simulate RST request from MainPCB
        comm_msg_t sim_msg = {
            .type = MSG_RST_REQUEST,
            .timestamp = now,
        };
        
        ESP_LOGI(TAG, "Simulating RST request from MainPCB");
        xQueueSend(comm_queue, &sim_msg, 0);
    }
    
    return SLAVE_PCB_OK;
}

/**
 * @brief Communication Manager initialization
 */
slave_pcb_err_t communication_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Communication Manager");

    slave_pcb_err_t ret = init_w5500_spi();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to initialize W5500 SPI");
        return ret;
    }

    ret = init_w5500_network();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to initialize W5500 network");
        return ret;
    }

    ESP_LOGI(TAG, "Communication Manager initialized successfully");
    return SLAVE_PCB_OK;
}

/**
 * @brief Communication Manager main task
 */
void communication_manager_task(void *pvParameters) {
    comm_msg_t msg;
    
    ESP_LOGI(TAG, "Communication Manager task started");

    while (1) {
        // Check for incoming messages from MainPCB
        receive_message_from_main_pcb();

        // Process messages from other managers
        if (xQueueReceive(button_queue, &msg, 0) == pdTRUE) {
            send_message_to_main_pcb(&msg);
        }

        if (xQueueReceive(loadcell_queue, &msg, 0) == pdTRUE) {
            send_message_to_main_pcb(&msg);
        }

        // Send periodic status updates
        static uint32_t last_status_update = 0;
        uint32_t now = esp_timer_get_time() / 1000;
        
        if (now - last_status_update > 5000) { // Every 5 seconds
            last_status_update = now;
            
            comm_msg_t status_msg = {
                .type = MSG_DEVICE_STATUS,
                .timestamp = now,
                .data.device_status = {
                    .device = DEVICE_PUMP_PE,
                    .status = true // TODO: Get actual pump status
                }
            };
            
            send_message_to_main_pcb(&status_msg);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
