#include "../main/slave_pcb.h"
#include "unity.h"

// Test fixtures
static QueueHandle_t test_queue;
static SemaphoreHandle_t test_mutex;

void setUp(void) {
    // Set up test environment
    test_queue = xQueueCreate(10, sizeof(comm_msg_t));
    test_mutex = xSemaphoreCreateMutex();
}

void tearDown(void) {
    // Clean up test environment
    if (test_queue) {
        vQueueDelete(test_queue);
        test_queue = NULL;
    }
    if (test_mutex) {
        vSemaphoreDelete(test_mutex);
        test_mutex = NULL;
    }
}

// Test case compatibility function
void test_case_compatibility(void) {
    // Test RST case (should be compatible with any state)
    TEST_ASSERT_TRUE(is_case_compatible(CASE_RST, 0xFFFFFFFF));
    TEST_ASSERT_TRUE(is_case_compatible(CASE_RST, 0));
    
    // Test E1 case (incompatible with CE and DF states)
    TEST_ASSERT_TRUE(is_case_compatible(CASE_E1, 0));
    TEST_ASSERT_FALSE(is_case_compatible(CASE_E1, STATE_CE));
    TEST_ASSERT_FALSE(is_case_compatible(CASE_E1, STATE_DF));
    TEST_ASSERT_FALSE(is_case_compatible(CASE_E1, STATE_CE | STATE_DF));
    TEST_ASSERT_TRUE(is_case_compatible(CASE_E1, STATE_DE | STATE_RF | STATE_RE));
    
    // Test V1 case (incompatible with DE state)
    TEST_ASSERT_TRUE(is_case_compatible(CASE_V1, 0));
    TEST_ASSERT_FALSE(is_case_compatible(CASE_V1, STATE_DE));
    TEST_ASSERT_TRUE(is_case_compatible(CASE_V1, STATE_CE | STATE_DF | STATE_RF | STATE_RE));
    
    // Test invalid case
    TEST_ASSERT_FALSE(is_case_compatible(CASE_MAX, 0));
    TEST_ASSERT_FALSE(is_case_compatible((system_case_t)99, 0));
}

// Test error string function
void test_error_strings(void) {
    TEST_ASSERT_EQUAL_STRING("Success", get_error_string(SLAVE_PCB_OK));
    TEST_ASSERT_EQUAL_STRING("Invalid argument", get_error_string(SLAVE_PCB_ERR_INVALID_ARG));
    TEST_ASSERT_EQUAL_STRING("Device not found", get_error_string(SLAVE_PCB_ERR_DEVICE_NOT_FOUND));
    TEST_ASSERT_EQUAL_STRING("Incompatible case", get_error_string(SLAVE_PCB_ERR_INCOMPATIBLE_CASE));
    TEST_ASSERT_EQUAL_STRING("Unknown error", get_error_string((slave_pcb_err_t)99));
}

// Test case string function
void test_case_strings(void) {
    TEST_ASSERT_EQUAL_STRING("RST", get_case_string(CASE_RST));
    TEST_ASSERT_EQUAL_STRING("E1", get_case_string(CASE_E1));
    TEST_ASSERT_EQUAL_STRING("E2", get_case_string(CASE_E2));
    TEST_ASSERT_EQUAL_STRING("D1", get_case_string(CASE_D1));
    TEST_ASSERT_EQUAL_STRING("V1", get_case_string(CASE_V1));
    TEST_ASSERT_EQUAL_STRING("V2", get_case_string(CASE_V2));
    TEST_ASSERT_EQUAL_STRING("P1", get_case_string(CASE_P1));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", get_case_string(CASE_MAX));
}

// Test device state functions
void test_device_states(void) {
    // Initialize shift registers for testing
    TEST_ASSERT_EQUAL(SLAVE_PCB_OK, init_shift_registers());
    
    // Test setting device states
    TEST_ASSERT_EQUAL(SLAVE_PCB_OK, shift_register_set_output_state(DEVICE_ELECTROVALVE_A, true));
    TEST_ASSERT_TRUE(get_device_state(DEVICE_ELECTROVALVE_A));
    
    TEST_ASSERT_EQUAL(SLAVE_PCB_OK, shift_register_set_output_state(DEVICE_ELECTROVALVE_A, false));
    TEST_ASSERT_FALSE(get_device_state(DEVICE_ELECTROVALVE_A));
    
    // Test multiple devices
    TEST_ASSERT_EQUAL(SLAVE_PCB_OK, shift_register_set_output_state(DEVICE_PUMP_PE, true));
    TEST_ASSERT_EQUAL(SLAVE_PCB_OK, shift_register_set_output_state(DEVICE_LED_BE1_RED, true));
    
    TEST_ASSERT_TRUE(get_device_state(DEVICE_PUMP_PE));
    TEST_ASSERT_TRUE(get_device_state(DEVICE_LED_BE1_RED));
    TEST_ASSERT_FALSE(get_device_state(DEVICE_ELECTROVALVE_A));
    
    // Test safe state
    TEST_ASSERT_EQUAL(SLAVE_PCB_OK, set_all_outputs_safe());
    TEST_ASSERT_FALSE(get_device_state(DEVICE_PUMP_PE));
    TEST_ASSERT_FALSE(get_device_state(DEVICE_LED_BE1_RED));
    
    // Test invalid device
    TEST_ASSERT_EQUAL(SLAVE_PCB_ERR_INVALID_ARG, 
                      shift_register_set_output_state(DEVICE_MAX, true));
    TEST_ASSERT_FALSE(get_device_state(DEVICE_MAX));
}

// Test communication message structure
void test_communication_messages(void) {
    comm_msg_t msg;
    
    // Test case change message
    msg.type = MSG_CASE_CHANGE;
    msg.timestamp = 12345;
    msg.data.case_data = CASE_E1;
    
    TEST_ASSERT_EQUAL(MSG_CASE_CHANGE, msg.type);
    TEST_ASSERT_EQUAL(12345, msg.timestamp);
    TEST_ASSERT_EQUAL(CASE_E1, msg.data.case_data);
    
    // Test button state message
    msg.type = MSG_BUTTON_STATE;
    msg.data.button_data.button = BUTTON_BE1;
    msg.data.button_data.state = true;
    
    TEST_ASSERT_EQUAL(MSG_BUTTON_STATE, msg.type);
    TEST_ASSERT_EQUAL(BUTTON_BE1, msg.data.button_data.button);
    TEST_ASSERT_TRUE(msg.data.button_data.state);
    
    // Test load cell data message
    msg.type = MSG_LOAD_CELL_DATA;
    msg.data.load_cell_data.tank = TANK_A;
    msg.data.load_cell_data.weight = 25.5f;
    
    TEST_ASSERT_EQUAL(MSG_LOAD_CELL_DATA, msg.type);
    TEST_ASSERT_EQUAL(TANK_A, msg.data.load_cell_data.tank);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.5f, msg.data.load_cell_data.weight);
}

// Test boundary conditions
void test_boundary_conditions(void) {
    // Test maximum values
    TEST_ASSERT_FALSE(is_case_compatible(CASE_MAX, 0));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", get_case_string(CASE_MAX));
    TEST_ASSERT_FALSE(get_device_state(DEVICE_MAX));
    
    // Test minimum values
    TEST_ASSERT_TRUE(is_case_compatible(CASE_RST, 0));
    TEST_ASSERT_EQUAL_STRING("RST", get_case_string(CASE_RST));
    
    // Test NULL pointer handling (where applicable)
    // Note: Some functions may not handle NULL pointers gracefully
    // This would depend on the specific implementation requirements
}

// Main test runner
void app_main(void) {
    printf("Starting SlavePCB Unit Tests\n");
    
    UNITY_BEGIN();
    
    // Run all test cases
    RUN_TEST(test_case_compatibility);
    RUN_TEST(test_error_strings);
    RUN_TEST(test_case_strings);
    RUN_TEST(test_device_states);
    RUN_TEST(test_communication_messages);
    RUN_TEST(test_boundary_conditions);
    
    UNITY_END();
    
    printf("All tests completed\n");
    
    // Keep the test runner alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
