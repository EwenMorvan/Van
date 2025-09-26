#include "w5500_ethernet.h"
#include "gpio_pinout.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "communication_protocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "W5500_ETH";
static spi_device_handle_t spi_handle;
static bool slave_connected = false;
static TaskHandle_t tcp_server_task_handle = NULL;
static QueueHandle_t received_command_queue = NULL;
static bool w5500_initialized = false;

// W5500 register addresses and values
#define W5500_COMMON_BASE           0x0000
#define W5500_SOCKET_BASE(n)        (0x0008 + (n) * 0x0100)
#define W5500_TXBUF_BASE(n)         (0x8000 + (n) * 0x0800)
#define W5500_RXBUF_BASE(n)         (0xC000 + (n) * 0x0800)

// Common registers
#define W5500_GAR                   0x0001  // Gateway Address
#define W5500_SUBR                  0x0005  // Subnet Mask  
#define W5500_SHAR                  0x0009  // Source Hardware Address
#define W5500_SIPR                  0x000F  // Source IP Address

// Socket registers (offset from socket base)
#define W5500_Sn_MR                 0x0000  // Socket Mode
#define W5500_Sn_CR                 0x0001  // Socket Command
#define W5500_Sn_IR                 0x0002  // Socket Interrupt
#define W5500_Sn_SR                 0x0003  // Socket Status
#define W5500_Sn_PORT               0x0004  // Socket Source Port
#define W5500_Sn_DHAR               0x0006  // Socket Destination Hardware Address
#define W5500_Sn_DIPR               0x000C  // Socket Destination IP Address
#define W5500_Sn_DPORT              0x0010  // Socket Destination Port
#define W5500_Sn_MSSR               0x0012  // Socket Maximum Segment Size
#define W5500_Sn_TOS                0x0015  // Socket Type of Service
#define W5500_Sn_TTL                0x0016  // Socket Time to Live
#define W5500_Sn_RXBUF_SIZE         0x001E  // Socket RX Buffer Size
#define W5500_Sn_TXBUF_SIZE         0x001F  // Socket TX Buffer Size
#define W5500_Sn_TX_FSR             0x0020  // Socket TX Free Size
#define W5500_Sn_TX_RD              0x0022  // Socket TX Read Pointer
#define W5500_Sn_TX_WR              0x0024  // Socket TX Write Pointer
#define W5500_Sn_RX_RSR             0x0026  // Socket RX Received Size
#define W5500_Sn_RX_RD              0x0028  // Socket RX Read Pointer
#define W5500_Sn_RX_WR              0x002A  // Socket RX Write Pointer

// Socket modes
#define W5500_S_MR_CLOSE            0x00
#define W5500_S_MR_TCP              0x01
#define W5500_S_MR_UDP              0x02

// Socket commands
#define W5500_S_CR_OPEN             0x01
#define W5500_S_CR_LISTEN           0x02
#define W5500_S_CR_CONNECT          0x04
#define W5500_S_CR_DISCON           0x08
#define W5500_S_CR_CLOSE            0x10
#define W5500_S_CR_SEND             0x20
#define W5500_S_CR_RECV             0x40

// Socket status
#define W5500_S_SR_CLOSED           0x00
#define W5500_S_SR_INIT             0x13
#define W5500_S_SR_LISTEN           0x14
#define W5500_S_SR_ESTABLISHED      0x17
#define W5500_S_SR_CLOSE_WAIT       0x1C

#define COMMAND_QUEUE_SIZE 10
#define TCP_SERVER_BUFFER_SIZE 512

// Network configuration
static const uint8_t mac_addr[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x50};  // Different from SlavePCB
static const uint8_t ip_addr[4] = {192, 168, 1, 50};     // MainPCB IP from SlavePCB code
static const uint8_t subnet_mask[4] = {255, 255, 255, 0};
static const uint8_t gateway[4] = {192, 168, 1, 1};

// Function prototypes
static esp_err_t w5500_write_reg(uint16_t addr, uint8_t block, const uint8_t *data, uint16_t len);
static esp_err_t w5500_read_reg(uint16_t addr, uint8_t block, uint8_t *data, uint16_t len);
static esp_err_t w5500_init_network(void);
static esp_err_t w5500_socket_init(uint8_t socket, uint16_t port);
static esp_err_t w5500_socket_listen(uint8_t socket);
static int w5500_socket_recv(uint8_t socket, uint8_t *buffer, uint16_t len);
static void tcp_server_task(void *pvParameters);

esp_err_t w5500_ethernet_init(void) {
    esp_err_t ret;
    
    // Configure SPI bus
    spi_bus_config_t bus_config = {
        .mosi_io_num = SPI_MOSI,
        .miso_io_num = SPI_MISO,
        .sclk_io_num = SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024
    };
    
    ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure SPI device
    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
        .mode = 0,
        .spics_io_num = SPI_CS,
        .queue_size = 7
    };
    
    ret = spi_bus_add_device(SPI2_HOST, &dev_config, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure W5500 reset pin
    gpio_config_t reset_config = {
        .pin_bit_mask = (1ULL << W5500_RST_1),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&reset_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure reset pin: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Reset W5500
    gpio_set_level(W5500_RST_1, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(W5500_RST_1, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize W5500 network configuration
    ret = w5500_init_network();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize W5500 network");
        return ret;
    }
    
    // Initialize socket 0 for TCP server on port 8080
    ret = w5500_socket_init(0, 8080);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize socket");
        return ret;
    }
    
    // Start listening on socket 0
    ret = w5500_socket_listen(0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start listening");
        return ret;
    }
    
    // Create command queue for received commands
    received_command_queue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(van_command_t));
    if (received_command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
        return ESP_FAIL;
    }
    
    // Start TCP server task
    BaseType_t result = xTaskCreatePinnedToCore(
        tcp_server_task,
        "tcp_server",
        4096,
        NULL,
        5,
        &tcp_server_task_handle,
        1  // Pin to CPU1
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TCP server task");
        return ESP_FAIL;
    }
    
    w5500_initialized = true;
    ESP_LOGI(TAG, "W5500 Ethernet initialized and TCP server started on port 8080");
    return ESP_OK;
}

esp_err_t w5500_send_state(van_state_t *state) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Implement actual W5500 communication
    // For now, just log that we're sending state
    ESP_LOGD(TAG, "Sending state to SlavePCB");
    
    return ESP_OK;
}

esp_err_t w5500_receive_command(van_command_t *cmd) {
    if (cmd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if there's a command in the queue
    if (received_command_queue != NULL && 
        xQueueReceive(received_command_queue, cmd, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Command received from SlavePCB");
        return ESP_OK;
    }
    
    return ESP_ERR_NOT_FOUND;
}

bool w5500_is_slave_connected(void) {
    return slave_connected;
}

// W5500 SPI register access functions
static esp_err_t w5500_write_reg(uint16_t addr, uint8_t block, const uint8_t *data, uint16_t len) {
    if (len > 252) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    uint8_t tx_buf[259];
    tx_buf[0] = (addr >> 8) & 0xFF;
    tx_buf[1] = addr & 0xFF;
    tx_buf[2] = (block << 3) | 0x04; // Control byte for write
    memcpy(&tx_buf[3], data, len);
    
    spi_transaction_t trans = {
        .length = (3 + len) * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = NULL,
    };
    
    return spi_device_transmit(spi_handle, &trans);
}

static esp_err_t w5500_read_reg(uint16_t addr, uint8_t block, uint8_t *data, uint16_t len) {
    uint8_t tx_buf[259];
    uint8_t rx_buf[259];
    
    if (len > 256) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    tx_buf[0] = (addr >> 8) & 0xFF;
    tx_buf[1] = addr & 0xFF;
    tx_buf[2] = (block << 3) | 0x00; // Control byte for read
    
    // Fill remaining tx_buf with dummy data for the read phase
    for (int i = 3; i < (3 + len); i++) {
        tx_buf[i] = 0x00;
    }
    
    spi_transaction_t trans = {
        .length = (3 + len) * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };
    
    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Copy the received data (skip the first 3 bytes which are the command echo)
    memcpy(data, &rx_buf[3], len);
    
    return ESP_OK;
}

static esp_err_t w5500_init_network(void) {
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing W5500 network configuration");
    
    // Set MAC address
    ret = w5500_write_reg(W5500_SHAR, 0x00, mac_addr, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set MAC address");
        return ret;
    }
    
    // Set IP address
    ret = w5500_write_reg(W5500_SIPR, 0x00, ip_addr, 4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set IP address");
        return ret;
    }
    
    // Set subnet mask
    ret = w5500_write_reg(W5500_SUBR, 0x00, subnet_mask, 4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set subnet mask");
        return ret;
    }
    
    // Set gateway
    ret = w5500_write_reg(W5500_GAR, 0x00, gateway, 4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set gateway");
        return ret;
    }
    
    ESP_LOGI(TAG, "W5500 network configuration completed");
    ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    ESP_LOGI(TAG, "IP: %d.%d.%d.%d", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
    
    return ESP_OK;
}

static esp_err_t w5500_socket_init(uint8_t socket, uint16_t port) {
    esp_err_t ret;
    uint8_t block = (socket << 3) | 0x01; // Socket register block: (socket << 3) | RWB=0 | OM=01
    
    // Set socket mode to TCP
    uint8_t mode = W5500_S_MR_TCP;
    ret = w5500_write_reg(W5500_Sn_MR, block, &mode, 1);
    if (ret != ESP_OK) return ret;
    
    // Set local port
    uint8_t port_bytes[2] = {(port >> 8) & 0xFF, port & 0xFF};
    ret = w5500_write_reg(W5500_Sn_PORT, block, port_bytes, 2);
    if (ret != ESP_OK) return ret;
    
    // Open socket
    uint8_t cmd = W5500_S_CR_OPEN;
    ret = w5500_write_reg(W5500_Sn_CR, block, &cmd, 1);
    if (ret != ESP_OK) return ret;
    
    // Wait for socket to initialize
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ESP_LOGI(TAG, "Socket %d initialized on port %d", socket, port);
    return ESP_OK;
}

static esp_err_t w5500_socket_listen(uint8_t socket) {
    esp_err_t ret;
    uint8_t block = (socket << 3) | 0x01; // Socket register block
    
    // Listen command
    uint8_t cmd = W5500_S_CR_LISTEN;
    ret = w5500_write_reg(W5500_Sn_CR, block, &cmd, 1);
    if (ret != ESP_OK) return ret;
    
    ESP_LOGI(TAG, "Socket %d is now listening", socket);
    return ESP_OK;
}

static int w5500_socket_recv(uint8_t socket, uint8_t *buffer, uint16_t len) {
    esp_err_t ret;
    uint8_t block = (socket << 3) | 0x01; // Socket register block
    
    // Check received data size
    uint8_t rx_size[2];
    ret = w5500_read_reg(W5500_Sn_RX_RSR, block, rx_size, 2);
    if (ret != ESP_OK) return -1;
    
    uint16_t received_size = (rx_size[0] << 8) | rx_size[1];
    if (received_size == 0) {
        return 0; // No data
    }
    
    if (received_size > len) {
        received_size = len; // Limit to buffer size
    }
    
    // Read RX read pointer
    uint8_t rx_rd[2];
    ret = w5500_read_reg(W5500_Sn_RX_RD, block, rx_rd, 2);
    if (ret != ESP_OK) return -1;
    
    uint16_t rx_ptr = (rx_rd[0] << 8) | rx_rd[1];
    
    // Read data from RX buffer using proper RX buffer access
    uint8_t tx_buf[259];
    uint8_t rx_buf[259];
    
    if (received_size > 256) {
        received_size = 256; // Limit to max transfer size
    }
    
    // RX buffer access with control byte for RX buffer read
    uint16_t buffer_addr = rx_ptr & 0x1FFF; // Mask to get buffer offset
    tx_buf[0] = (buffer_addr >> 8) & 0xFF;
    tx_buf[1] = buffer_addr & 0xFF;
    tx_buf[2] = (socket << 3) | 0x03; // RX buffer read (BSB=socket, RWB=0, OM=11)
    
    // Fill remaining tx_buf with dummy data for the read phase
    for (int i = 3; i < (3 + received_size); i++) {
        tx_buf[i] = 0x00;
    }
    
    spi_transaction_t trans = {
        .length = (3 + received_size) * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };
    
    ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) return -1;
    
    // Copy the received data (skip the first 3 bytes which are the command echo)
    memcpy(buffer, &rx_buf[3], received_size);
    
    // Update RX read pointer
    rx_ptr += received_size;
    rx_rd[0] = (rx_ptr >> 8) & 0xFF;
    rx_rd[1] = rx_ptr & 0xFF;
    ret = w5500_write_reg(W5500_Sn_RX_RD, block, rx_rd, 2);
    if (ret != ESP_OK) return -1;
    
    // Send RECV command
    uint8_t cmd = W5500_S_CR_RECV;
    ret = w5500_write_reg(W5500_Sn_CR, block, &cmd, 1);
    if (ret != ESP_OK) return -1;
    
    return received_size;
}

static void tcp_server_task(void *pvParameters) {
    ESP_LOGI(TAG, "TCP server task started");
    
    uint8_t buffer[TCP_SERVER_BUFFER_SIZE];
    uint8_t socket_status;
    uint8_t block = (0 << 3) | 0x01; // Socket 0 register block
    
    while (w5500_initialized) {
        // Check socket status
        esp_err_t ret = w5500_read_reg(W5500_Sn_SR, block, &socket_status, 1);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read socket status");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        switch (socket_status) {
            case W5500_S_SR_ESTABLISHED:
                if (!slave_connected) {
                    ESP_LOGI(TAG, "Client connected");
                    slave_connected = true;
                }
                
                // Try to receive data
                int bytes_received = w5500_socket_recv(0, buffer, TCP_SERVER_BUFFER_SIZE - 1);
                if (bytes_received > 0) {
                    buffer[bytes_received] = '\0';
                    ESP_LOGI(TAG, "Received: '%s' (%d bytes)", buffer, bytes_received);
                    
                    // Try to parse as JSON command
                    cJSON *json = cJSON_Parse((char*)buffer);
                    if (json != NULL) {
                        van_command_t cmd;
                        esp_err_t parse_result = comm_parse_command((char*)buffer, &cmd);
                        
                        if (parse_result == ESP_OK && received_command_queue != NULL) {
                            xQueueSend(received_command_queue, &cmd, pdMS_TO_TICKS(100));
                            ESP_LOGI(TAG, "Command queued successfully");
                        }
                        
                        cJSON_Delete(json);
                    } else {
                        // Handle non-JSON messages like "hello world"
                        ESP_LOGI(TAG, "Received message from SlavePCB: %s", buffer);
                    }
                }
                break;
                
            case W5500_S_SR_CLOSE_WAIT:
                ESP_LOGI(TAG, "Client disconnecting");
                slave_connected = false;
                
                // Close socket
                uint8_t cmd = W5500_S_CR_DISCON;
                w5500_write_reg(W5500_Sn_CR, block, &cmd, 1);
                break;
                
            case W5500_S_SR_CLOSED:
                if (slave_connected) {
                    ESP_LOGI(TAG, "Socket closed, restarting listener");
                    slave_connected = false;
                }
                
                // Reinitialize socket
                w5500_socket_init(0, 8080);
                w5500_socket_listen(0);
                break;
                
            case W5500_S_SR_LISTEN:
                // Waiting for connection
                break;
                
            default:
                ESP_LOGD(TAG, "Socket status: 0x%02X", socket_status);
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "TCP server task ending");
    tcp_server_task_handle = NULL;
    vTaskDelete(NULL);
}
