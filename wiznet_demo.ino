#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
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

IPAddress remoteIP(10, 0, 0, 122); // Change to IP of Motor Control
EthernetUDP Udp;

// Pin definitions
#define IR_SENSOR_A_PIN 2  // Forward Sensor
#define IR_SENSOR_B_PIN 10   // Backward Sensor

// Timing
#define SEND_INTERVAL 2000 // ms
#define DEBOUNCE 10000 // us
#define SPOKES_PER_REV 8
#define DIRECTION_WINDOW 20000 // us

// Shared state
volatile int32_t g_spoke_count = 0;
static bool last_a_state = false;
static bool last_b_state = false;
static uint32_t last_trigger = 0; 
static uint32_t b_last = 0;
repeating_timer_t timer;

/*
// Pack int32 as big-endian into 4 bytes
static void pack_be_int32(uint8_t* buf, int32_t value)
{
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >>  8) & 0xFF;
    buf[3] = value & 0xFF;
}
*/
// Sensor polling — called every 1ms by repeating timer
bool sensor_callback(repeating_timer_t *rt)
{
    bool a = gpio_get(IR_SENSOR_A_PIN);
    bool b = gpio_get(IR_SENSOR_B_PIN);
    uint32_t now = time_us_32();

    // Track when B last went high
    if (b == true && last_b_state == false) {
        b_last = now;
    }
    last_b_state = b;

    // On A rising edge
    if (a == true && last_a_state == false) {
        if (now - last_trigger > DEBOUNCE) {
            last_trigger = now;
            // If B went high recently before A, B led -> reverse
            // If B is not high yet, A led -> forward
            if (b == true && (now - b_last < DIRECTION_WINDOW)) {
                g_spoke_count--;  // reverse, B led
            } else {
                g_spoke_count++;  // forward, A led
            }
        }
    }
    last_a_state = a;
    return true;
}

void setup()
{
    Serial.begin(9600);
    while (!Serial) delay(10);

    // GPIO setup
    pinMode(IR_SENSOR_A_PIN, INPUT);
    pinMode(IR_SENSOR_B_PIN, INPUT);

    // Start 1ms polling timer
    add_repeating_timer_ms(1, sensor_callback, NULL, &timer);

    Serial.println("Tachometer ready.");

    /*

    // WizNet setup
    Ethernet.init(csPin);
    Serial.print("Attempting to obtain IP address using DHCP\n");
    if (Ethernet.begin(mac) == 0) {
        Serial.print("Failed to obtain IP address using DHCP, aborting\n");
        while (true);
    } else {
        Serial.print("Successfully obtained IP address: ");
        Serial.println(Ethernet.localIP());
    }

    Udp.begin(LOCAL_PORT_NUMBER);
    Serial.println("WizNet setup complete.");
    */
}

void loop()
{
    delay(SEND_INTERVAL);

    // Snapshot and reset spoke counter
    noInterrupts();
    int32_t count = g_spoke_count;
    g_spoke_count = 0;
    interrupts();

    /*
    // Pack as big-endian int32
    uint8_t payload[4];
    pack_be_int32(payload, count);
    */
    // Debug output
    float rpm = ((float)abs(count) / (float)SPOKES_PER_REV) * (1000.0f / SEND_INTERVAL) * 60.0f;
    Serial.printf("Spokes: %4d | RPM: %7.1f | Dir: %s\n",
        int(count), rpm,
        count > 0 ? "FWD" : (count < 0 ? "REV" : "STOP"));

    /*
    // Send data over network
    Udp.beginPacket(remoteIP, REMOTE_PORT_NUMBER);
    Udp.write(payload, 4);
    Udp.endPacket();
    */
}