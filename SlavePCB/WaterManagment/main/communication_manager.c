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
        .flags = 0, // Remove halfduplex flag - W5500 uses full duplex
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
    uint8_t tx_buf[259]; // Max 3 bytes command + 256 bytes data
    uint8_t rx_buf[259];
    
    if (len > 256) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // W5500 SPI frame: [Address High][Address Low][Control Byte][Data...]
    tx_buf[0] = (addr >> 8) & 0xFF;
    tx_buf[1] = addr & 0xFF;
    tx_buf[2] = 0x00; // Control byte for common register read (BSB=00000, RWB=0, OM=00)
    
    // Fill remaining tx_buf with dummy data for the read phase
    for (int i = 3; i < (3 + len); i++) {
        tx_buf[i] = 0x00;
    }
    
    spi_transaction_t trans = {
        .length = (3 + len) * 8, // Command + data length in bits
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };

    esp_err_t ret = spi_device_transmit(w5500_spi, &trans);
    if (ret != ESP_OK) {
        return ret;
    }

    // Copy the received data (skip the first 3 bytes which are the command echo)
    memcpy(data, &rx_buf[3], len);
    
    return ESP_OK;
}

/**
 * @brief Write register to W5500
 */
static esp_err_t w5500_write_reg(uint16_t addr, const uint8_t *data, size_t len) {
    uint8_t tx_buf[259]; // Max 3 bytes command + 256 bytes data
    if (len > 256) {
        return ESP_ERR_INVALID_SIZE;
    }

    // W5500 SPI frame: [Address High][Address Low][Control Byte][Data...]
    tx_buf[0] = (addr >> 8) & 0xFF;
    tx_buf[1] = addr & 0xFF;
    tx_buf[2] = 0x04; // Control byte for common register write (BSB=00000, RWB=1, OM=00)
    memcpy(&tx_buf[3], data, len);

    spi_transaction_t trans = {
        .length = (3 + len) * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = NULL, // No data expected back for write operations
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

// W5500 Socket Configuration
#define W5500_SOCKET_0           0
#define W5500_TCP_MODE           0x01
#define W5500_SOCKET_INIT        0x13
#define W5500_SOCKET_LISTEN      0x14
#define W5500_SOCKET_ESTABLISHED 0x17
#define W5500_SOCKET_CLOSE_WAIT  0x1C
#define W5500_SOCKET_CLOSED      0x00

// MainPCB connection details
#define MAIN_PCB_IP {192, 168, 1, 50}
#define MAIN_PCB_PORT 8080
#define SLAVE_PCB_PORT 8081

/**
 * @brief Read W5500 socket register
 */
static esp_err_t w5500_read_socket_reg(uint8_t socket_num, uint16_t addr, uint8_t *data, size_t len) {
    uint8_t tx_buf[259]; // Max 3 bytes command + 256 bytes data
    uint8_t rx_buf[259];
    
    if (len > 256) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // W5500 Socket register addressing
    tx_buf[0] = (addr >> 8) & 0xFF;
    tx_buf[1] = addr & 0xFF;
    tx_buf[2] = (socket_num << 5) | 0x08; // Socket register read (BSB=socket_num, RWB=0, OM=01)
    
    // Fill remaining tx_buf with dummy data for the read phase
    for (int i = 3; i < (3 + len); i++) {
        tx_buf[i] = 0x00;
    }
    
    spi_transaction_t trans = {
        .length = (3 + len) * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };

    esp_err_t ret = spi_device_transmit(w5500_spi, &trans);
    if (ret != ESP_OK) {
        return ret;
    }

    // Copy the received data (skip the first 3 bytes which are the command echo)
    memcpy(data, &rx_buf[3], len);
    
    return ESP_OK;
}

/**
 * @brief Write W5500 socket register
 */
static esp_err_t w5500_write_socket_reg(uint8_t socket_num, uint16_t addr, const uint8_t *data, size_t len) {
    uint8_t tx_buf[259]; // Max 3 bytes command + 256 bytes data
    if (len > 256) {
        return ESP_ERR_INVALID_SIZE;
    }

    // W5500 Socket register addressing
    tx_buf[0] = (addr >> 8) & 0xFF;
    tx_buf[1] = addr & 0xFF;
    tx_buf[2] = (socket_num << 5) | 0x0C; // Socket register write (BSB=socket_num, RWB=1, OM=01)
    memcpy(&tx_buf[3], data, len);

    spi_transaction_t trans = {
        .length = (3 + len) * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = NULL, // No data expected back for write operations
    };

    return spi_device_transmit(w5500_spi, &trans);
}
/**
 * @brief Open TCP socket on W5500
 */
static esp_err_t w5500_socket_open(uint8_t socket_num, uint16_t local_port) {
    esp_err_t ret;
    
    // Set socket mode to TCP
    uint8_t mode = W5500_TCP_MODE;
    ret = w5500_write_socket_reg(socket_num, 0x0000, &mode, 1);
    if (ret != ESP_OK) return ret;
    
    // Set local port
    uint8_t port_bytes[2] = {(local_port >> 8) & 0xFF, local_port & 0xFF};
    ret = w5500_write_socket_reg(socket_num, 0x0004, port_bytes, 2);
    if (ret != ESP_OK) return ret;
    
    // Open socket
    uint8_t cmd = W5500_SOCKET_INIT;
    ret = w5500_write_socket_reg(socket_num, 0x0001, &cmd, 1);
    if (ret != ESP_OK) return ret;
    
    // Wait for socket to initialize
    vTaskDelay(pdMS_TO_TICKS(10));
    
    return ESP_OK;
}

/**
 * @brief Connect to remote TCP server
 */
static esp_err_t w5500_socket_connect(uint8_t socket_num, const uint8_t *remote_ip, uint16_t remote_port) {
    esp_err_t ret;
    
    // Set destination IP
    ret = w5500_write_socket_reg(socket_num, 0x000C, remote_ip, 4);
    if (ret != ESP_OK) return ret;
    
    // Set destination port
    uint8_t port_bytes[2] = {(remote_port >> 8) & 0xFF, remote_port & 0xFF};
    ret = w5500_write_socket_reg(socket_num, 0x0010, port_bytes, 2);
    if (ret != ESP_OK) return ret;
    
    // Connect command
    uint8_t cmd = 0x04; // CONNECT command
    ret = w5500_write_socket_reg(socket_num, 0x0001, &cmd, 1);
    if (ret != ESP_OK) return ret;
    
    // Wait for connection (with timeout)
    for (int i = 0; i < 100; i++) { // 1 second timeout
        uint8_t status;
        ret = w5500_read_socket_reg(socket_num, 0x0003, &status, 1);
        if (ret != ESP_OK) return ret;
        
        if (status == W5500_SOCKET_ESTABLISHED) {
            ESP_LOGI(TAG, "TCP connection established");
            return ESP_OK;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGE(TAG, "TCP connection timeout");
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Send data through TCP socket
 */
static esp_err_t w5500_socket_send(uint8_t socket_num, const uint8_t *data, uint16_t len) {
    esp_err_t ret;
    
    // Check if socket is connected
    uint8_t status;
    ret = w5500_read_socket_reg(socket_num, 0x0003, &status, 1);
    if (ret != ESP_OK || status != W5500_SOCKET_ESTABLISHED) {
        ESP_LOGE(TAG, "Socket not connected (status: 0x%02X)", status);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get free buffer size
    uint8_t free_size[2];
    ret = w5500_read_socket_reg(socket_num, 0x0020, free_size, 2);
    if (ret != ESP_OK) return ret;
    
    uint16_t free_bytes = (free_size[0] << 8) | free_size[1];
    if (free_bytes < len) {
        ESP_LOGE(TAG, "Not enough buffer space (need %d, have %d)", len, free_bytes);
        return ESP_ERR_NO_MEM;
    }
    
    // Get current write pointer
    uint8_t wr_ptr[2];
    ret = w5500_read_socket_reg(socket_num, 0x0024, wr_ptr, 2);
    if (ret != ESP_OK) return ret;
    
    uint16_t write_ptr = (wr_ptr[0] << 8) | wr_ptr[1];
    
    // Write data to TX buffer using proper TX buffer access
    uint8_t tx_buf[259]; // Max 3 bytes command + 256 bytes data
    if (len > 256) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // TX buffer access with control byte for TX buffer write
    // Use the masked write pointer for the buffer address
    uint16_t buffer_addr = write_ptr & 0x1FFF; // Mask to get buffer offset
    tx_buf[0] = (buffer_addr >> 8) & 0xFF;
    tx_buf[1] = buffer_addr & 0xFF;
    tx_buf[2] = (socket_num << 5) | 0x14; // TX buffer write (BSB=socket_num, RWB=1, OM=10)
    memcpy(&tx_buf[3], data, len);
    
    spi_transaction_t trans = {
        .length = (3 + len) * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = NULL,
    };
    
    ret = spi_device_transmit(w5500_spi, &trans);
    if (ret != ESP_OK) return ret;
    
    // Update write pointer
    write_ptr += len;
    uint8_t new_wr_ptr[2] = {(write_ptr >> 8) & 0xFF, write_ptr & 0xFF};
    ret = w5500_write_socket_reg(socket_num, 0x0024, new_wr_ptr, 2);
    if (ret != ESP_OK) return ret;
    
    // Send command
    uint8_t cmd = 0x20; // SEND command
    ret = w5500_write_socket_reg(socket_num, 0x0001, &cmd, 1);
    if (ret != ESP_OK) return ret;
    
    ESP_LOGI(TAG, "Sent %d bytes via TCP", len);
    return ESP_OK;
}

/**
 * @brief Close TCP socket
 */
static esp_err_t w5500_socket_close(uint8_t socket_num) {
    uint8_t cmd = 0x10; // CLOSE command
    return w5500_write_socket_reg(socket_num, 0x0001, &cmd, 1);
}

/**
 * @brief Send message to MainPCB via Ethernet
 */
static slave_pcb_err_t send_message_to_main_pcb(const comm_msg_t *msg) {
    if (!msg) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Sending message type %d to MainPCB", msg->type);

    // For now, send "hello world" as requested
    const char *hello_msg = "hello world";
    const uint8_t main_pcb_ip[] = MAIN_PCB_IP;
    
    esp_err_t ret;
    
    // Open TCP socket
    ret = w5500_socket_open(W5500_SOCKET_0, SLAVE_PCB_PORT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open socket: %s", esp_err_to_name(ret));
        return SLAVE_PCB_ERR_COMM_FAIL;
    }
    
    // Connect to MainPCB
    ret = w5500_socket_connect(W5500_SOCKET_0, main_pcb_ip, MAIN_PCB_PORT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to MainPCB: %s", esp_err_to_name(ret));
        w5500_socket_close(W5500_SOCKET_0);
        return SLAVE_PCB_ERR_COMM_FAIL;
    }
    
    // Send hello world message
    ret = w5500_socket_send(W5500_SOCKET_0, (const uint8_t*)hello_msg, strlen(hello_msg));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send data: %s", esp_err_to_name(ret));
        w5500_socket_close(W5500_SOCKET_0);
        return SLAVE_PCB_ERR_COMM_FAIL;
    }
    
    ESP_LOGI(TAG, "Successfully sent: %s", hello_msg);
    
    // Close socket
    w5500_socket_close(W5500_SOCKET_0);
    
    // Also log the original message for debugging
    switch (msg->type) {
        case MSG_CASE_CHANGE:
            ESP_LOGI(TAG, "Original message - case change: %s", get_case_string(msg->data.case_data));
            break;
        case MSG_BUTTON_STATE:
            ESP_LOGI(TAG, "Original message - button state: Button %d = %s", 
                     msg->data.button_data.button, 
                     msg->data.button_data.state ? "PRESSED" : "RELEASED");
            break;
        case MSG_LOAD_CELL_DATA:
            ESP_LOGI(TAG, "Original message - load cell data: Tank %d = %.2f kg", 
                     msg->data.load_cell_data.tank, 
                     msg->data.load_cell_data.weight);
            break;
        case MSG_DEVICE_STATUS:
            ESP_LOGI(TAG, "Original message - device status: Device %d = %s", 
                     msg->data.device_status.device, 
                     msg->data.device_status.status ? "OK" : "FAULT");
            break;
        case MSG_ERROR:
            ESP_LOGI(TAG, "Original message - error: %s", msg->data.error_data.description);
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
