#include "w5500_comm.h"
#include "gpio_pinout.h"  // Pour les définitions des pins
#include <string.h>
#include <esp_log.h>
#include <driver/spi_master.h>  // Pour SPI
#include <esp_eth.h>  // Pour Ethernet
#include <esp_netif.h>  // Pour netif
#include <esp_eth_netif_glue.h>  // Pour esp_eth_new_netif_glue
#include <driver/gpio.h>  // Pour GPIO ISR service
#include <sys/socket.h>   // Pour sockets
#include <netinet/in.h>   // Pour sockaddr_in
#include <arpa/inet.h>    // Pour inet_pton
#include <errno.h>        // Pour errno
#include <unistd.h>       // Pour close()

static const char *TAG = "W5500_COMM";

// Test avancé de communication SPI avec W5500
static esp_err_t test_w5500_spi_communication(spi_host_device_t host, gpio_num_t cs_gpio) {
    ESP_LOGI(TAG, "=== Testing W5500 SPI Communication ===");
    
    // Configuration SPI device simple pour test
    spi_device_interface_config_t test_devcfg = {
        .mode = 0,                          // SPI mode 0
        .clock_speed_hz = 1000000,          // 1 MHz pour test
        .spics_io_num = cs_gpio,
        .queue_size = 1
    };
    
    spi_device_handle_t spi_handle;
    esp_err_t ret = spi_bus_add_device(host, &test_devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Test 1: Lecture registre version (0x0039) - doit retourner 0x04
    ESP_LOGI(TAG, "Test 1: Reading Version Register (0x0039)");
    uint8_t tx_version[4] = {0x00, 0x39, 0x00, 0x00}; // [ADDR_H][ADDR_L][CTRL][DATA]
    uint8_t rx_version[4] = {0};
    
    spi_transaction_t trans_version = {
        .length = 32,
        .tx_buffer = tx_version,
        .rx_buffer = rx_version
    };
    
    ret = spi_device_transmit(spi_handle, &trans_version);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Version TX: %02X %02X %02X %02X", tx_version[0], tx_version[1], tx_version[2], tx_version[3]);
        ESP_LOGI(TAG, "Version RX: %02X %02X %02X %02X", rx_version[0], rx_version[1], rx_version[2], rx_version[3]);
        ESP_LOGI(TAG, "Version expected: 0x04, got: 0x%02X", rx_version[3]);
    } else {
        ESP_LOGE(TAG, "Version read failed: %s", esp_err_to_name(ret));
    }

    // Test 2: Lecture registre RTR (Retry Time Register) - adresse 0x0019-0x001A  
    ESP_LOGI(TAG, "Test 2: Reading RTR Register (0x0019)");
    uint8_t tx_rtr[4] = {0x00, 0x19, 0x00, 0x00};
    uint8_t rx_rtr[4] = {0};
    
    spi_transaction_t trans_rtr = {
        .length = 32,
        .tx_buffer = tx_rtr,
        .rx_buffer = rx_rtr
    };
    
    ret = spi_device_transmit(spi_handle, &trans_rtr);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "RTR TX: %02X %02X %02X %02X", tx_rtr[0], tx_rtr[1], tx_rtr[2], tx_rtr[3]);
        ESP_LOGI(TAG, "RTR RX: %02X %02X %02X %02X", rx_rtr[0], rx_rtr[1], rx_rtr[2], rx_rtr[3]);
    }
    
    // Test 3: Tentative d'écriture puis lecture d'un registre
    ESP_LOGI(TAG, "Test 3: Write/Read test on RTR register");
    // Écrire 0x07D0 (2000 en décimal) dans RTR
    uint8_t tx_write[4] = {0x00, 0x19, 0x04, 0x07}; // Mode écriture (0x04)
    uint8_t rx_write[4] = {0};
    
    spi_transaction_t trans_write = {
        .length = 32,
        .tx_buffer = tx_write,
        .rx_buffer = rx_write
    };
    
    ret = spi_device_transmit(spi_handle, &trans_write);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write result: %s", (ret == ESP_OK) ? "OK" : "FAILED");
    }
    
    // Test 4: Test avec différentes fréquences
    spi_bus_remove_device(spi_handle);
    
    // Test à fréquence très basse
    ESP_LOGI(TAG, "Test 4: Very low frequency test (100 kHz)");
    test_devcfg.clock_speed_hz = 100000; // 100 kHz
    ret = spi_bus_add_device(host, &test_devcfg, &spi_handle);
    if (ret == ESP_OK) {
        ret = spi_device_transmit(spi_handle, &trans_version);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "100kHz Version RX: %02X %02X %02X %02X", rx_version[0], rx_version[1], rx_version[2], rx_version[3]);
        }
        spi_bus_remove_device(spi_handle);
    }
    
    ESP_LOGI(TAG, "=== W5500 SPI Test Complete ===");
    
    // Analyser les résultats
    if (rx_version[3] == 0x04) {
        ESP_LOGI(TAG, "✅ W5500 detected and responding correctly!");
        return ESP_OK;
    } else if (rx_version[0] == 0x00 && rx_version[1] == 0x00 && rx_version[2] == 0x00 && rx_version[3] == 0x00) {
        ESP_LOGE(TAG, "❌ W5500 not responding - check power supply and connections");
        ESP_LOGE(TAG, "Hardware checklist:");
        ESP_LOGE(TAG, "1. 3.3V power supply connected and stable?");
        ESP_LOGE(TAG, "2. All GND connections secure?");
        ESP_LOGE(TAG, "3. SPI pins correctly connected?");
        ESP_LOGE(TAG, "4. Reset pin (GPIO 2) connected and functioning?");
        return ESP_FAIL;
    } else if (rx_version[0] == 0xFF && rx_version[1] == 0xFF && rx_version[2] == 0xFF && rx_version[3] == 0xFF) {
        ESP_LOGE(TAG, "❌ SPI communication issue - all bits high (floating lines?)");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "❌ W5500 responding but wrong version: 0x%02X (expected 0x04)", rx_version[3]);
        ESP_LOGE(TAG, "This could indicate:");
        ESP_LOGE(TAG, "1. Wrong chip (not W5500)");
        ESP_LOGE(TAG, "2. Damaged W5500");
        ESP_LOGE(TAG, "3. Incorrect SPI protocol");
        return ESP_FAIL;
    }
}

// Callback pour événements Ethernet
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    w5500_comm_t *comm = (w5500_comm_t *)arg;
    if (event_base == ETH_EVENT) {
        if (event_id == ETHERNET_EVENT_CONNECTED) {
            ESP_LOGI(TAG, "Ethernet connected");
            esp_netif_t *netif = comm->netif;
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ip_info.ip));
            }
        } else if (event_id == ETHERNET_EVENT_DISCONNECTED) {
            ESP_LOGW(TAG, "Ethernet disconnected");
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_ETH_GOT_IP) {
            ESP_LOGI(TAG, "Got IP");
        }
    }
}

// Configuration SPI pour W5500
static esp_eth_mac_t *w5500_mac_new(spi_host_device_t host, gpio_num_t cs_gpio, gpio_num_t int_gpio, gpio_num_t rst_gpio) {
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 4096;  // Augmenté pour S3
    mac_config.rx_task_prio = 12;          // Priorité haute
    
    // Configuration du SPI device - fréquence très basse pour debug
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,                          // SPI mode 0
        .clock_speed_hz = 500000,           // 500 kHz très lent pour debug
        .spics_io_num = cs_gpio,
        .queue_size = 20
    };
    
    // Configuration W5500 manuelle (comme dans l'exemple qui fonctionne)
    eth_w5500_config_t w5500_config = {
        .int_gpio_num = int_gpio,           // GPIO pour interrupt (-1 si non connecté)
        .poll_period_ms = (int_gpio == -1) ? 50 : 0,  // Polling si pas d'interrupt
        .spi_host_id = host,                // SPI host
        .spi_devcfg = &spi_devcfg          // Configuration SPI device
    };
    
    return esp_eth_mac_new_w5500(&w5500_config, &mac_config);
}

// Configuration PHY (W5500 intègre MAC+PHY)
static esp_eth_phy_t *w5500_phy_new(void) {
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = -1;  // PHY address pour W5500
    phy_config.reset_gpio_num = -1;  // -1 signifie pas de reset hardware
    return esp_eth_phy_new_w5500(&phy_config);
}

esp_err_t w5500_comm_init(w5500_comm_t *comm, uint8_t is_server, const char *remote_ip, uint16_t port) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Installer le service GPIO ISR pour les interruptions
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Configuration SPI avec vos bonnes pins
    spi_bus_config_t bus_cfg = {
        .miso_io_num = SPI_MISO,     // SPI_MISO (13)
        .mosi_io_num = SPI_MOSI,     // SPI_MOSI (11)
        .sclk_io_num = SPI_CLK,      // SPI_CLK (12)
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized successfully");

    // MAC W5500 avec vos bonnes pins
    gpio_num_t cs_gpio = SPI_CS;   // SPI_CS (10)
    gpio_num_t int_gpio = -1;      // INT pin non connecté pour l'instant
    gpio_num_t rst_gpio = W5500_RST; // W5500_RST pin (2)
    
    // Reset manuel du W5500 avec séquence extended
    ESP_LOGI(TAG, "Performing W5500 hardware reset...");
    gpio_config_t rst_gpio_config = {
        .pin_bit_mask = (1ULL << rst_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&rst_gpio_config));
    
    // Séquence de reset très longue pour W5500
    ESP_LOGI(TAG, "Step 1: Set RST HIGH (inactive) - Initial state");
    gpio_set_level(rst_gpio, 1);  // S'assurer que reset est inactif d'abord
    vTaskDelay(pdMS_TO_TICKS(100)); // Plus de temps initial
    
    ESP_LOGI(TAG, "Step 2: Set RST LOW (active reset) - Hold reset");
    gpio_set_level(rst_gpio, 0);  // Reset actif
    vTaskDelay(pdMS_TO_TICKS(500)); // Reset très long (500ms)
    
    ESP_LOGI(TAG, "Step 3: Set RST HIGH (release reset) - Wait for boot");
    gpio_set_level(rst_gpio, 1);  // Relâcher reset
    vTaskDelay(pdMS_TO_TICKS(1000)); // Beaucoup plus de temps pour stabilisation (1s)
    
    ESP_LOGI(TAG, "W5500 hardware reset completed - should be ready now");
    
    // Test de communication SPI basique avant l'initialisation
    ESP_LOGI(TAG, "Testing SPI communication with W5500...");
    esp_err_t spi_test_result = test_w5500_spi_communication(SPI2_HOST, cs_gpio);
    if (spi_test_result != ESP_OK) {
        ESP_LOGE(TAG, "SPI communication test failed - check hardware connections");
        ESP_LOGE(TAG, "Verify: CS=%d, MOSI=%d, MISO=%d, CLK=%d, RST=%d", 
                 cs_gpio, SPI_MOSI, SPI_MISO, SPI_CLK, rst_gpio);
        ESP_LOGE(TAG, "Also check W5500 power supply (3.3V) and ground connections");
        // Continue malgré l'échec pour voir l'erreur exacte
    }
    
    ESP_LOGI(TAG, "Initializing W5500 MAC with pins: CS=%d, MOSI=%d, MISO=%d, CLK=%d", 
             cs_gpio, SPI_MOSI, SPI_MISO, SPI_CLK);
    
    esp_eth_mac_t *mac = w5500_mac_new(SPI2_HOST, cs_gpio, int_gpio, rst_gpio);
    if (!mac) {
        ESP_LOGE(TAG, "Failed to init MAC - Check SPI connections and W5500 power");
        return ESP_FAIL;
    }

    // PHY W5500
    esp_eth_phy_t *phy = w5500_phy_new();
    if (!phy) {
        ESP_LOGE(TAG, "Failed to init PHY");
        mac->del(mac);
        return ESP_FAIL;
    }

    // Configuration Ethernet
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    esp_err_t install_result = esp_eth_driver_install(&eth_config, &eth_handle);
    if (install_result != ESP_OK) {
        ESP_LOGE(TAG, "❌ HARDWARE PROBLEM: W5500 not responding to SPI commands");
        ESP_LOGE(TAG, "This is definitely a hardware issue. Check:");
        ESP_LOGE(TAG, "1. W5500 power supply (3.3V stable)");
        ESP_LOGE(TAG, "2. All GND connections");
        ESP_LOGE(TAG, "3. SPI pin connections (solder joints)");
        ESP_LOGE(TAG, "4. W5500 reset pin connection");
        ESP_LOGE(TAG, "5. W5500 chip may be damaged");
        
        // Nettoyage avant de retourner l'erreur
        phy->del(phy);
        mac->del(mac);
        spi_bus_free(SPI2_HOST);
        return ESP_FAIL;
    }
    if (!eth_handle) {
        ESP_LOGE(TAG, "Failed to install Ethernet driver");
        phy->del(phy);
        mac->del(mac);
        return ESP_FAIL;
    }

    // Créer l'interface réseau
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *netif = esp_netif_new(&netif_cfg);
    if (!netif) {
        ESP_LOGE(TAG, "Failed to create netif");
        esp_eth_driver_uninstall(eth_handle);
        phy->del(phy);
        mac->del(mac);
        return ESP_FAIL;
    }

    // Attacher l'interface Ethernet à netif
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_eth_new_netif_glue(eth_handle)));

    // Enregistrez handlers d'événements
    ESP_ERROR_CHECK(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, comm, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_event_handler, comm, NULL));

    // Arrêter DHCP avant de configurer l'IP statique
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));
    
    // IP statique (adaptez le réseau)
    esp_netif_ip_info_t ip_info;
    if (is_server) {
        IP4_ADDR(&ip_info.ip, 192, 168, 1, 10);
        IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        comm->local_port = port;
    } else {
        IP4_ADDR(&ip_info.ip, 192, 168, 1, 11);
        IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        comm->local_port = port;
    }
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
    
    ESP_LOGI(TAG, "IP configured: %s", is_server ? "192.168.1.10" : "192.168.1.11");

    // Socket TCP
    comm->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (comm->socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        esp_eth_driver_uninstall(eth_handle);
        esp_netif_destroy(netif);
        phy->del(phy);
        mac->del(mac);
        return ESP_FAIL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(comm->local_port);
    inet_pton(AF_INET, is_server ? "0.0.0.0" : remote_ip, &addr.sin_addr);

    int opt = 1;
    setsockopt(comm->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (is_server) {
        // Mode serveur : listen
        if (bind(comm->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ESP_LOGE(TAG, "Bind failed: %d", errno);
            close(comm->socket);
            esp_eth_driver_uninstall(eth_handle);
            esp_netif_destroy(netif);
            phy->del(phy);
            mac->del(mac);
            return ESP_FAIL;
        }
        if (listen(comm->socket, 1) < 0) {
            ESP_LOGE(TAG, "Listen failed: %d", errno);
            close(comm->socket);
            esp_eth_driver_uninstall(eth_handle);
            esp_netif_destroy(netif);
            phy->del(phy);
            mac->del(mac);
            return ESP_FAIL;
        }
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(comm->socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Accept failed: %d", errno);
            close(comm->socket);
            esp_eth_driver_uninstall(eth_handle);
            esp_netif_destroy(netif);
            phy->del(phy);
            mac->del(mac);
            return ESP_FAIL;
        }
        close(comm->socket);  // Fermer socket serveur
        comm->socket = client_sock;
        ESP_LOGI(TAG, "Client connected");
    } else {
        // Mode client : connect
        addr.sin_port = htons(port);
        if (connect(comm->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ESP_LOGE(TAG, "Connect failed: %d", errno);
            close(comm->socket);
            esp_eth_driver_uninstall(eth_handle);
            esp_netif_destroy(netif);
            phy->del(phy);
            mac->del(mac);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Connected to server");
    }

    // Démarrer l'interface Ethernet
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    comm->eth_handle = eth_handle;
    comm->netif = netif;
    comm->mutex = xSemaphoreCreateMutex();
    comm->is_server = is_server;
    comm->remote_ip = remote_ip;
    comm->remote_port = port;

    ESP_LOGI(TAG, "W5500 comm initialized (server=%d)", is_server);
    return ESP_OK;
}

void w5500_comm_deinit(w5500_comm_t *comm) {
    if (comm->socket >= 0) {
        close(comm->socket);
    }
    if (comm->eth_handle) {
        esp_eth_stop(comm->eth_handle);
        esp_eth_driver_uninstall(comm->eth_handle);
    }
    if (comm->netif) {
        esp_netif_destroy(comm->netif);
    }
    if (comm->mutex) {
        vSemaphoreDelete(comm->mutex);
    }
    spi_bus_free(SPI2_HOST);
    ESP_LOGI(TAG, "W5500 comm deinitialized");
}

esp_err_t w5500_comm_send(w5500_comm_t *comm, const char *msg, size_t len) {
    if (xSemaphoreTake(comm->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    int sent = send(comm->socket, msg, len, 0);
    xSemaphoreGive(comm->mutex);
    if (sent < 0) {
        ESP_LOGE(TAG, "Send failed: %d", errno);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Sent: %.*s", len, msg);
    return ESP_OK;
}

esp_err_t w5500_comm_recv(w5500_comm_t *comm, char *buf, size_t buf_len, size_t *recv_len) {
    if (xSemaphoreTake(comm->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    int bytes_received = recv(comm->socket, buf, buf_len - 1, MSG_DONTWAIT);
    xSemaphoreGive(comm->mutex);
    if (bytes_received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            *recv_len = 0;
            return ESP_ERR_NOT_FOUND;  // Pas de données
        }
        ESP_LOGE(TAG, "Recv failed: %d", errno);
        *recv_len = 0;
        return ESP_FAIL;
    }
    *recv_len = (size_t)bytes_received;
    buf[*recv_len] = '\0';
    ESP_LOGI(TAG, "Received: %s", buf);
    return ESP_OK;
}