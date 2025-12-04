#include "ethernet_manager.h"


static const char *TAG = "ETHERNET";

// Use pins from gpio_pinout.h
#define PIN_ETH_MISO SPI_MISO
#define PIN_ETH_MOSI SPI_MOSI
#define PIN_ETH_CLK  SPI_CLK
#define PIN_ETH_CS   SPI_CS
#define PIN_ETH_INT  -1
#define PIN_ETH_RST  W5500_RST

// Internal state
static esp_netif_t *eth_netif = NULL;
static esp_eth_handle_t eth_handle = NULL;
static EventGroupHandle_t eth_event_group;
static SemaphoreHandle_t ethernet_ready_semaphore = NULL;
static const int ETH_CONNECTED_BIT = BIT0;
static const int IP_READY_BIT = BIT1;
static int udp_socket = -1;
static ethernet_receive_callback_t receive_callback = NULL;
static ethernet_config_t current_config;

// Client configuration
const ethernet_config_t ETHERNET_CLIENT_CONFIG = {
    .is_server = false,
    .ip_address = "192.168.1.101",
    .netmask = "255.255.255.0",
    .gateway = "192.168.1.1",
    .port = 8888,
    .mac_address = {0x02, 0x00, 0x00, 0x01, 0x01, 0x02},
    .receive_callback = NULL
};
// Server configuration
const ethernet_config_t ETHERNET_SERVER_CONFIG = {
    .is_server = true,
    .ip_address = "192.168.1.100",
    .netmask = "255.255.255.0",
    .gateway = "192.168.1.1",
    .port = 8888,
    .mac_address = {0x02, 0x00, 0x00, 0x01, 0x01, 0x01},
    .receive_callback = NULL
};


// Forward declarations
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static esp_err_t configure_static_ip(void);
static void udp_receive_task(void *pvParameters);
static void wait_for_ip_configuration(void);

// Ethernet event handler
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Up");
            xEventGroupSetBits(eth_event_group, ETH_CONNECTED_BIT);
            break;
            
        case ETHERNET_EVENT_DISCONNECTED:
            // ESP_LOGW(TAG, "Ethernet Link Down");
            REPORT_ERROR(SLAVE_PCB_ERR_ETH_DISCONNECTED, TAG, "Ethernet disconnected", 0);
            xEventGroupClearBits(eth_event_group, ETH_CONNECTED_BIT | IP_READY_BIT);
            break;
            
        default:
            break;
    }
}

// IP event handler
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
        case IP_EVENT_ETH_GOT_IP:
            {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
                ESP_LOGD(TAG, "Got DHCP IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
                // For static IP, we don't rely on this event
            }
            break;
            
        case IP_EVENT_ETH_LOST_IP:
            ESP_LOGD(TAG, "Ethernet Lost IP");
            xEventGroupClearBits(eth_event_group, IP_READY_BIT);
            break;
            
        default:
            break;
    }
}

// Configure static IP
static esp_err_t configure_static_ip(void)
{
    esp_netif_ip_info_t ip_info;
    
    // Stop DHCP client first
    esp_err_t ret = esp_netif_dhcpc_stop(eth_netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        // ESP_LOGE(TAG, "Failed to stop DHCP client: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to stop DHCP client", 0);
        return ret;
    }
    
    // Set static IP configuration
    memset(&ip_info, 0, sizeof(ip_info));
    ip_info.ip.addr = esp_ip4addr_aton(current_config.ip_address);
    ip_info.netmask.addr = esp_ip4addr_aton(current_config.netmask);
    ip_info.gw.addr = esp_ip4addr_aton(current_config.gateway);
    
    ret = esp_netif_set_ip_info(eth_netif, &ip_info);
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "Failed to set static IP: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to set static IP", 0);
        return ret;
    }
    
    ESP_LOGD(TAG, "Static IP configured: %s", current_config.ip_address);
    return ESP_OK;
}

// Wait for IP configuration to be active
static void wait_for_ip_configuration(void)
{
    ESP_LOGD(TAG, "Waiting for IP configuration to be active...");
    
    // Check periodically if IP is configured
    for (int i = 0; i < 50; i++) { // 5 seconds max
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(eth_netif, &ip_info) == ESP_OK) {
            if (ip_info.ip.addr != 0) {
                ESP_LOGD(TAG, "IP configuration active: " IPSTR, IP2STR(&ip_info.ip));
                xEventGroupSetBits(eth_event_group, IP_READY_BIT);
                return;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGW(TAG, "IP configuration check timeout, continuing anyway...");
    REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "IP configuration timeout", 0);
    // Continue even if timeout is reached
    xEventGroupSetBits(eth_event_group, IP_READY_BIT);
}

// UDP receive task
static void udp_receive_task(void *pvParameters)
{
    ESP_LOGD(TAG, "UDP receive task started");
    
    // Wait for IP to be ready (with shorter timeout)
    ESP_LOGD(TAG, "Waiting for IP configuration...");
    EventBits_t bits = xEventGroupWaitBits(eth_event_group, IP_READY_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(3000));
    if (!(bits & IP_READY_BIT)) {
        ESP_LOGW(TAG, "IP configuration wait timeout, continuing anyway...");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "IP configuration wait timeout", 0);
    }

    // Create UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        // ESP_LOGE(TAG, "Failed to create UDP socket: errno %d", errno);
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to create UDP socket", errno);
        if (ethernet_ready_semaphore != NULL) {
            xSemaphoreGive(ethernet_ready_semaphore);
        }
        vTaskDelete(NULL);
        return;
    }

    // Set socket to non-blocking
    int flags = fcntl(udp_socket, F_GETFL, 0);
    fcntl(udp_socket, F_SETFL, flags | O_NONBLOCK);

    // Bind socket
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(current_config.port),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        // ESP_LOGE(TAG, "Failed to bind socket: errno %d", errno);
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to bind UDP socket", errno);
        close(udp_socket);
        udp_socket = -1;
        if (ethernet_ready_semaphore != NULL) {
            xSemaphoreGive(ethernet_ready_semaphore);
        }
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGD(TAG, "UDP socket bound to port %d", current_config.port);
    
    // Small delay to ensure network stack is ready
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Signal that Ethernet is ready
    if (ethernet_ready_semaphore != NULL) {
        xSemaphoreGive(ethernet_ready_semaphore);
    }

    ESP_LOGD(TAG, "Ethernet fully ready for communication");

    // Main receive loop
    while (1) {
        if (udp_socket < 0) break;
        
        uint8_t buffer[1500]; // MTU size
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        
        int len = recvfrom(udp_socket, buffer, sizeof(buffer), 0, 
                         (struct sockaddr *)&source_addr, &addr_len);
        
        if (len > 0) {
            // Data received
            if (receive_callback != NULL) {
                char source_ip[16];
                inet_ntoa_r(source_addr.sin_addr, source_ip, sizeof(source_ip) - 1);
                receive_callback(buffer, len, source_ip, ntohs(source_addr.sin_port));
            }
        } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // ESP_LOGE(TAG, "recvfrom error: %d", errno);
            REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to receive UDP packet", errno);
            break;
        }
        
        // Small delay to prevent CPU hogging
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (udp_socket >= 0) {
        close(udp_socket);
        udp_socket = -1;
    }
    
    vTaskDelete(NULL);
}

// Initialize Ethernet
esp_err_t ethernet_manager_init(const ethernet_config_t *config)
{
    if (config == NULL) {
        // ESP_LOGE(TAG, "Invalid Ethernet configuration (NULL)");
        REPORT_ERROR(SLAVE_PCB_ERR_INVALID_ARG, TAG, "Invalid Ethernet configuration (NULL)", 0);
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&current_config, config, sizeof(ethernet_config_t));
    receive_callback = config->receive_callback;

    ESP_LOGI(TAG, "Initializing Ethernet...");
    
    // Create event group
    eth_event_group = xEventGroupCreate();
    if (eth_event_group == NULL) {
        // ESP_LOGE(TAG, "Failed to create event group");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to create event group", 0);
        return ESP_FAIL;
    }

    // Create semaphore
    ethernet_ready_semaphore = xSemaphoreCreateBinary();
    if (ethernet_ready_semaphore == NULL) {
        // ESP_LOGE(TAG, "Failed to create ready semaphore");
        vEventGroupDelete(eth_event_group);
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to create ready semaphore", 0);
        return ESP_FAIL;
    }

    // Initialize networking stack
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to initialize netif", 0);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ret;
    }
    
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to create event loop", 0);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ret;
    }

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL));

    // Create network interface
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&netif_cfg);
    if (eth_netif == NULL) {
        // ESP_LOGE(TAG, "Failed to create network interface");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to create network interface", 0);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ESP_FAIL;
    }

    // Reset W5500
    gpio_reset_pin(PIN_ETH_RST);
    gpio_set_direction(PIN_ETH_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_ETH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_ETH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Initialize SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_ETH_MOSI,
        .miso_io_num = PIN_ETH_MISO,
        .sclk_io_num = PIN_ETH_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "SPI bus initialization failed: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "SPI bus initialization failed", 0);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ret;
    }

    // Configure SPI device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_ETH_CS,
        .queue_size = 20,
    };
    
    spi_device_handle_t spi_handle;
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "SPI device addition failed: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "SPI device addition failed", 0);
        spi_bus_free(SPI2_HOST);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ret;
    }

    // Configure W5500
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = -1;
    phy_config.reset_gpio_num = PIN_ETH_RST;

    eth_w5500_config_t w5500_config = {
        .int_gpio_num = PIN_ETH_INT,
        .poll_period_ms = 50,
        .spi_host_id = SPI2_HOST,
        .spi_devcfg = &devcfg
    };
    
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (mac == NULL) {
        // ESP_LOGE(TAG, "Failed to create MAC instance");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to create MAC instance", 0);
        spi_bus_remove_device(spi_handle);
        spi_bus_free(SPI2_HOST);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ESP_FAIL;
    }

    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (phy == NULL) {
        // ESP_LOGE(TAG, "Failed to create PHY instance");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Failed to create PHY instance", 0);
        if (mac->del != NULL) mac->del(mac);
        spi_bus_remove_device(spi_handle);
        spi_bus_free(SPI2_HOST);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ESP_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    eth_config.check_link_period_ms = 1000;

    ret = esp_eth_driver_install(&eth_config, &eth_handle);
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "Ethernet driver installation failed: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Ethernet driver installation failed", 0);
        if (phy->del != NULL) phy->del(phy);
        if (mac->del != NULL) mac->del(mac);
        spi_bus_remove_device(spi_handle);
        spi_bus_free(SPI2_HOST);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ret;
    }

    // Set MAC address
    esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, current_config.mac_address);

    // Attach to network interface
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    esp_netif_attach(eth_netif, glue);

    // Configure static IP immediately after attaching
    ret = configure_static_ip();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Static IP configuration had issues, continuing...");

    }

    // Start Ethernet
    ret = esp_eth_start(eth_handle);
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "Ethernet start failed: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Ethernet start failed", 0);
        esp_eth_driver_uninstall(eth_handle);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ret;
    }

    // Wait for Ethernet connection
    ESP_LOGD(TAG, "Waiting for Ethernet connection...");
    EventBits_t bits = xEventGroupWaitBits(eth_event_group, ETH_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
    if (!(bits & ETH_CONNECTED_BIT)) {
        // ESP_LOGE(TAG, "Ethernet connection timeout");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Ethernet connection timeout", 0);
        esp_eth_driver_uninstall(eth_handle);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ESP_ERR_TIMEOUT;
    }

    // Wait for IP configuration to be active
    wait_for_ip_configuration();

    // Start UDP receive task
    xTaskCreate(udp_receive_task, "udp_rx", 4096, NULL, 5, NULL);

    // Wait for UDP socket to be ready (timeout 10 seconds)
    if (xSemaphoreTake(ethernet_ready_semaphore, pdMS_TO_TICKS(10000)) == pdTRUE) {
        ESP_LOGI(TAG, "Ethernet initialization completed successfully");
        vSemaphoreDelete(ethernet_ready_semaphore);
        ethernet_ready_semaphore = NULL;
        return ESP_OK;
    } else {
        // ESP_LOGE(TAG, "Ethernet initialization timeout - UDP socket not ready");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Ethernet initialization timeout - UDP socket not ready", 0);
        esp_eth_driver_uninstall(eth_handle);
        vEventGroupDelete(eth_event_group);
        vSemaphoreDelete(ethernet_ready_semaphore);
        return ESP_ERR_TIMEOUT;
    }
}

// Send data via Ethernet
esp_err_t ethernet_send(const uint8_t *data, uint32_t length, const char *dest_ip, uint16_t dest_port)
{
    if (data == NULL || length == 0) {
        // ESP_LOGE(TAG, "Invalid send parameters");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Invalid send parameters", 0);
        return ESP_ERR_INVALID_ARG;
    }

    // Check if socket is ready and network is up
    if (udp_socket < 0) {
        // ESP_LOGE(TAG, "Socket not ready");   
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Socket not ready", 0);
        return ESP_ERR_INVALID_STATE;
    }

    if (!ethernet_is_connected()) {
        // ESP_LOGE(TAG, "Ethernet not connected");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Ethernet not connected", 0);
        return ESP_ERR_INVALID_STATE;
    }

    struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(dest_port),
        .sin_addr.s_addr = inet_addr(dest_ip)
    };

    // Check if IP address is valid
    if (dest_addr.sin_addr.s_addr == INADDR_NONE) {
        // ESP_LOGE(TAG, "Invalid destination IP: %s", dest_ip);
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Invalid destination IP", dest_ip);
        return ESP_ERR_INVALID_ARG;
    }

    int sent = sendto(udp_socket, data, length, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (sent < 0) {
        // ESP_LOGE(TAG, "Send failed: errno %d", errno);
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Send failed", errno);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Sent %d bytes to %s:%d", sent, dest_ip, dest_port);
    return ESP_OK;
}

// Send broadcast data
esp_err_t ethernet_send_broadcast(const uint8_t *data, uint32_t length, uint16_t dest_port)
{
    if (udp_socket < 0) {
        // ESP_LOGE(TAG, "Socket not ready");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Socket not ready", 0);
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!ethernet_is_connected()) {
        // ESP_LOGE(TAG, "Ethernet not connected");
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Ethernet not connected", 0);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Enable broadcast
    int broadcast_enable = 1;
    setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
    
    struct sockaddr_in broadcast_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(dest_port),
        .sin_addr.s_addr = 0xFFFFFFFF // 255.255.255.255
    };

    int sent = sendto(udp_socket, data, length, 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
    
    // Disable broadcast after sending
    broadcast_enable = 0;
    setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
    
    if (sent < 0) {
        // ESP_LOGE(TAG, "Broadcast send failed: errno %d", errno);
        REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Broadcast send failed", errno);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Sent %d bytes via broadcast to port %d", sent, dest_port);
    return ESP_OK;
}

// Set receive callback
void ethernet_set_receive_callback(ethernet_receive_callback_t callback)
{
    receive_callback = callback;
}

// Check if Ethernet is connected
bool ethernet_is_connected(void)
{
    return (xEventGroupGetBits(eth_event_group) & ETH_CONNECTED_BIT) != 0;
}

// Get current IP address
esp_err_t ethernet_get_ip_address(char *buffer, uint32_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(eth_netif, &ip_info);
    if (ret == ESP_OK) {
        snprintf(buffer, buffer_size, IPSTR, IP2STR(&ip_info.ip));
        return ESP_OK;
    } else {
        buffer[0] = '\0';
        return ret;
    }
}

// Get MAC address
esp_err_t ethernet_get_mac_address(uint8_t *mac)
{
    if (mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac);
}