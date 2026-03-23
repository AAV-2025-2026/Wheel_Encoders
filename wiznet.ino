#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/critical_section.h"
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

// Network Stuff
#define LOCAL_PORT_NUMBER 11101
#define REMOTE_PORT_NUMBER 10111

byte mac[] = {
  'T', 'e', 'm', 'p', 'e', 'l'
};

#define csPin 17 // Chip select pin, this is needed for the WizNet
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; // Buffer to hold data to send // Could be smaller

IPAddress remoteIP(192, 168, 1, 104); // Change to IP of Motor Control
EthernetUDP Udp;

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
void setup(void)
{
    Serial.begin(9600);
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

    Serial.printf("Tachometer started.\n");

    // WizNet Setup
    Ethernet.init(csPin);

    Serial.print("Attempting to obtain IP address using DHCP\n");
    if (Ethernet.begin(mac) == 0) {
      Serial.print("Failed to obtain IP address using DHCP, aborting\n");
      while (true); // Just loop forever at this point
    } else {
      Serial.print("Successfully obtained IP address: ");
      Serial.println(Ethernet.localIP());
    }

    Udp.begin(LOCAL_PORT_NUMBER);
    Serial.print("Wiznet Setup complete\n");
}

void loop() {
  // Main loop
  absolute_time_t next_window = make_timeout_time_ms(SEND_INTERVAL_MS);
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
  Serial.printf("Spokes: %4d | RPM: %7.1f | Dir: %s\n",
          static_cast<int>(count), rpm,
          (count > 0 ? "FWD" : (count < 0 ? "REV" : "STOP")));

  // Send data over network
  Udp.beginPacket(remoteIP, REMOTE_PORT_NUMBER);
  Udp.write(payload, 4);
  Udp.endPacket();
}
