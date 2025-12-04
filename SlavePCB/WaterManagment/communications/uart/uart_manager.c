#include "uart_manager.h"


static const char *TAG = "UART_MGR";

static QueueHandle_t uart_queue;
static uint32_t button_states = 0; // Bitmask pour les états des boutons
static TickType_t last_cmd_time = 0;
static const uint32_t BUTTON_TIMEOUT_MS = 200; // 200 ms sans commande = bouton relâché

// Mapping des commandes UART vers les boutons
typedef struct {
    const char* cmd;
    uart_button_cmd_t button;
} uart_cmd_mapping_t;

static const uart_cmd_mapping_t cmd_mappings[] = {
    {"BE1_CLICK", UART_CMD_BUTTON_E1},
    {"BE2_CLICK", UART_CMD_BUTTON_E2},
    {"BD1_CLICK", UART_CMD_BUTTON_D1},
    {"BD2_CLICK", UART_CMD_BUTTON_D2},
    {"BH_CLICK", UART_CMD_BUTTON_BH},
    {"BV1_CLICK", UART_CMD_BUTTON_V1},
    {"BV2_CLICK", UART_CMD_BUTTON_V2},
    {"BP1_CLICK", UART_CMD_BUTTON_P1},
    {"BRST_CLICK", UART_CMD_BUTTON_RST},
};

static uart_button_cmd_t parse_uart_command(const char* data) {
    if (data == NULL || data[0] == '\0') {
        return UART_CMD_UNKNOWN;
    }

    // Trouver la longueur utile (jusqu'au premier \r ou \n)
    int useful_length = 0;
    while (data[useful_length] != '\0' && 
           data[useful_length] != '\r' && 
           data[useful_length] != '\n') {
        useful_length++;
    }

    // Copier la partie utile
    char clean_cmd[20];
    int j = 0;
    for (int i = 0; i < useful_length && j < sizeof(clean_cmd) - 1; i++) {
        if (data[i] != ' ' && data[i] != '\t') {
            clean_cmd[j++] = data[i];
        }
    }
    clean_cmd[j] = '\0';

    ESP_LOGD(TAG, "Cleaned command: '%s' (from '%.*s')", clean_cmd, useful_length, data);

    // Vérifier si la commande n'est pas vide
    if (j == 0) {
        return UART_CMD_UNKNOWN;
    }

    // Chercher la commande dans le mapping
    for (int i = 0; i < sizeof(cmd_mappings) / sizeof(cmd_mappings[0]); i++) {
        if (strcmp(clean_cmd, cmd_mappings[i].cmd) == 0) {
            ESP_LOGI(TAG, "UART command parsed: '%s' -> %d", clean_cmd, cmd_mappings[i].button);
            return cmd_mappings[i].button;
        }
    }
    
    ESP_LOGW(TAG, "Unknown UART command: '%s'", clean_cmd);
    return UART_CMD_UNKNOWN;
}

static void uart_reader_task(void *pvParameters) {
    uint8_t data[128];
    
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, sizeof(data) - 1, pdMS_TO_TICKS(50));
        if (len > 0) {
            data[len] = '\0';
            ESP_LOGD(TAG, "UART received: '%s'", (char*)data);
            
            // Parser la commande
            uart_button_cmd_t button_cmd = parse_uart_command((char*)data);
            if (button_cmd != UART_CMD_UNKNOWN) {
                // Mettre à jour l'état du bouton
                button_states |= (1 << button_cmd);
                last_cmd_time = xTaskGetTickCount();
                ESP_LOGD(TAG, "Button %d activated via UART", button_cmd);
            }
        }
        
        // Vérifier les timeouts
        TickType_t current_time = xTaskGetTickCount();
        if (current_time - last_cmd_time > pdMS_TO_TICKS(BUTTON_TIMEOUT_MS)) {
            if (button_states != 0) {
                ESP_LOGD(TAG, "UART button timeout, clearing all states");
                button_states = 0;
            }
        }
    }
}

slave_pcb_err_t uart_manager_init(void) {
    // Configuration UART
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_RX_BUFFER_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, 
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Initialiser les états
    button_states = 0;
    last_cmd_time = xTaskGetTickCount();
    
    // Démarrer la tâche de lecture
    xTaskCreate(uart_reader_task, "uart_reader", 2048 * 2, NULL, 2, NULL);
    
    ESP_LOGI(TAG, "UART manager initialized");
    return SLAVE_PCB_OK;
}

bool uart_manager_get_button_state(uart_button_cmd_t button) {
    if (button >= UART_CMD_UNKNOWN) {
        return false;
    }
    
    // Vérifier le timeout
    TickType_t current_time = xTaskGetTickCount();
    if (current_time - last_cmd_time > pdMS_TO_TICKS(BUTTON_TIMEOUT_MS)) {
        button_states = 0;
        return false;
    }
    
    return (button_states & (1 << button)) != 0;
}

void uart_manager_clear_button_states(void) {
    button_states = 0;
    ESP_LOGD(TAG, "UART button states cleared");
}