
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/critical_section.h"
#include "port_common.h"
#include "wizchip_conf.h"
#include "wizchip_spi.h"
#include "socket.h"

// ABS function
#define abs(x) ((x) < 0 ? -(x) : (x))

// Network Definitions
#define PORT_NUMBER 5000

static wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip = {192, 168, 11, 2},                     // IP address
    .sn = {255, 255, 255, 0},                    // Subnet Mask
    .gw = {192, 168, 11, 1},                     // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
    .dhcp = NETINFO_STATIC
};

static uint8_t destination_ip[4] = {192, 168, 11, 4}; // Change this to Motor Control Pi

// Pin definitions
#define IR_SENSOR_A_PIN     14      // Interrupt pin
#define IR_SENSOR_B_PIN     15      // Direction phase pin

// Timing
#define SEND_INTERVAL_MS    100     // Must match 'interval' in LiveSpeedNode (0.1s)

// Shared state
static volatile int32_t g_spoke_count = 0;
static critical_section_t g_crit;


// Pack int32 as big-endian into 4 bytes
static void pack_be_int32(uint8_t* buf, int32_t value)
{
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >>  8) & 0xFF;
    buf[3] =  value        & 0xFF;
}


// GPIO ISR
/**
 * Fires on every falling edge of Sensor A (spoke detected).
 * Reads Sensor B at that instant to determine direction:
 *   B is LOW  -> Sensor A leading -> forward  -> +1
 *   B is HIGH -> Sensor B leading -> reverse  -> -1
 */
static void gpio_isr(uint gpio, uint32_t events)
{
    if (gpio == IR_SENSOR_A_PIN && (events & GPIO_IRQ_EDGE_FALL))
    {
        bool b_high = gpio_get(IR_SENSOR_B_PIN);

        critical_section_enter_blocking(&g_crit);
        if (!b_high) {
            g_spoke_count++;    // forward
        } else {
            g_spoke_count--;    // reverse
        }
        critical_section_exit(&g_crit);
    }
}


// Main
int main(void)
{
    stdio_init_all();
    critical_section_init(&g_crit);

    // GPIO setup
    gpio_init(IR_SENSOR_A_PIN);
    gpio_set_dir(IR_SENSOR_A_PIN, GPIO_IN);
    gpio_pull_up(IR_SENSOR_A_PIN);

    gpio_init(IR_SENSOR_B_PIN);
    gpio_set_dir(IR_SENSOR_B_PIN, GPIO_IN);
    gpio_pull_up(IR_SENSOR_B_PIN);

    gpio_set_irq_enabled_with_callback(IR_SENSOR_A_PIN,
                                       GPIO_IRQ_EDGE_FALL,
                                       true,
                                       &gpio_isr);

    // WizNet Setup
    wizchip_init(NULL, NULL);
    wizchip_setnetinfo(&g_net_info);
    int sock = 0;
    socket(sock, Sn_MR_UDP, PORT_NUMBER, 0);

    printf("Tachometer started.\n");

    // Main loop
    absolute_time_t next_window = make_timeout_time_ms(SEND_INTERVAL_MS);

    while (true)
    {
        sleep_until(next_window);
        next_window = make_timeout_time_ms(SEND_INTERVAL_MS);

        // snapshot and reset the spoke counter
        critical_section_enter_blocking(&g_crit);
        int32_t count = g_spoke_count;
        g_spoke_count = 0;
        critical_section_exit(&g_crit);

        // Pack as big-endian int32
        uint8_t payload[4];
        pack_be_int32(payload, count);

        // Debug output
        float rpm = ((float)abs(count) / 8.0f) * (1000.0f / SEND_INTERVAL_MS) * 60.0f;
        printf("Spokes: %4d | RPM: %7.1f | Dir: %s\n",
               count, rpm,
               (count > 0 ? "FWD" : (count < 0 ? "REV" : "STOP")));

        // Send over network
        int return_value = sendto(sock, payload, sizeof(payload), destination_ip, PORT_NUMBER);
        if (return_value < 0) {
            printf("Error sending data. Error code: %d\n", return_value);
        }
    }

    return 0;
}
