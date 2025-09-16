#include <reg51.h> // Standard Keil C51 header for AT89C51

// Traffic Light LED pins (Port 1)
sbit RED_LIGHT    = P1^0;
sbit YELLOW_LIGHT = P1^1;
sbit GREEN_LIGHT  = P1^2;

// Button Pins (Port 3)
sbit PEDESTRIAN_BUTTON_PIN = P3^2; // INT0 - Hardware: P3.2 with pull-up resistor, button to GND
sbit MANUAL_YELLOW_SWITCH  = P3^3; // Hardware: P3.3 with pull-up resistor, button to GND
sbit MANUAL_GREEN_SWITCH   = P3^4; // Hardware: P3.4 with pull-up resistor, button to GND

// Global flag for pedestrian request
bit pedestrian_request_active = 0;

// States for automatic sequence
#define STATE_GREEN  0
#define STATE_YELLOW 1
#define STATE_RED    2
unsigned char current_auto_state = STATE_RED; // Start with RED initially

// Timers for automatic sequence (values are counts for delay_ms(100))
// Each tick is approximately 100ms.
unsigned int green_duration_ticks  = 150; // Approx 15 seconds (150 * 100ms)
unsigned int yellow_duration_ticks = 30;  // Approx 3 seconds (30 * 100ms)
unsigned int red_duration_ticks    = 100; // Approx 10 seconds (100 * 100ms)
unsigned int current_state_timer_ticks = 0;

// Function for a simple software delay
// IMPORTANT: Calibrate the inner loop (the '120') based on your crystal frequency
// to get an approximate 1ms delay for each ms_count.
void delay_ms(unsigned int ms_count) {
    unsigned int i, j;
    for (i = 0; i < ms_count; i++) {
        for (j = 0; j < 120; j++); // For ~11-12MHz crystal, this is roughly 1ms
    }
}

// External Interrupt 0 Service Routine (ISR) for Pedestrian Button
void Pedestrian_ISR(void) interrupt 0 { // INT0 uses interrupt vector 0
    pedestrian_request_active = 1;
    // Basic debounce delay. For real hardware, more robust debouncing might be needed.
    delay_ms(50);
}

// Function to handle the pedestrian override sequence
void handle_pedestrian_override(void) {
    RED_LIGHT    = 1; // All vehicle lights RED
    YELLOW_LIGHT = 0;
    GREEN_LIGHT  = 0;

    // 
    delay_ms(10000); // Pedestrian crossing time (e.g., 10 seconds)

    pedestrian_request_active = 0;    // Clear the pedestrian request flag
    current_auto_state = STATE_RED;   // Default to RED state after pedestrian crossing
    current_state_timer_ticks = 0;    // Reset the timer for the automatic sequence
}

// Main function
void main(void) {
    // Initialize LED states: RED ON, Yellow and Green OFF at startup
    RED_LIGHT    = 1;
    YELLOW_LIGHT = 0;
    GREEN_LIGHT  = 0;

    // Initialize Port 3 pins for button inputs.
    // With external pull-up resistors, these pins will be HIGH when buttons are not pressed.
    // Writing '1' to a port pin configures it as an input.
    PEDESTRIAN_BUTTON_PIN = 1; // Initialize P3.2 as input
    MANUAL_YELLOW_SWITCH  = 1; // Initialize P3.3 as input
    MANUAL_GREEN_SWITCH   = 1; // Initialize P3.4 as input

    // Configure External Interrupt 0 (INT0 for P3.2 - Pedestrian Button)
    IT0 = 1;  // Configure INT0 for FALLING edge-triggered interrupt.
              // This matches your PULL-UP resistor hardware for the pedestrian button
              // (button press pulls pin LOW).
    EX0 = 1;  // Enable External Interrupt 0.
    EA  = 1;  // Enable Global Interrupt flag (master switch for all interrupts).

    current_auto_state = STATE_RED;        // Set initial state for automatic sequence
    current_state_timer_ticks = 0;       // Reset state timer

    while (1) { // Main operational loop
        // Priority 1: Pedestrian Request
        if (pedestrian_request_active) {
            handle_pedestrian_override();
            // After the pedestrian sequence, the loop restarts,
            // and the automatic sequence will begin from its defined post-pedestrian state (RED).
        }
        // Priority 2: Manual Yellow Override Switch (Active LOW due to PULL-UP resistor)
        else if (MANUAL_YELLOW_SWITCH == 0) { // Button pressed
            RED_LIGHT    = 0;
            YELLOW_LIGHT = 1;
            GREEN_LIGHT  = 0;
            // When a manual override is active, pause the automatic sequence's timer
            // by not incrementing current_state_timer_ticks in this branch.
            // You might also want to set current_auto_state to reflect this manual state
            // if other parts of your code depend on it, though it's not strictly necessary
            // for this "hold to override" logic.
            // current_auto_state = STATE_YELLOW; // Optional: reflect current manual state
            current_state_timer_ticks = 0; // Reset auto timer to prevent immediate change on release
        }
        // Priority 3: Manual Green Override Switch (Active LOW due to PULL-UP resistor)
        else if (MANUAL_GREEN_SWITCH == 0) { // Button pressed
            RED_LIGHT    = 0;
            YELLOW_LIGHT = 0;
            GREEN_LIGHT  = 1;
            // current_auto_state = STATE_GREEN; // Optional: reflect current manual state
            current_state_timer_ticks = 0; // Reset auto timer
        }
        // Priority 4: Automatic Traffic Light Sequence (if no overrides are active)
        else {
            current_state_timer_ticks++; // Increment the timer for the current automatic state

            if (current_auto_state == STATE_RED) {
                RED_LIGHT    = 1;
                YELLOW_LIGHT = 0;
                GREEN_LIGHT  = 0;
                if (current_state_timer_ticks >= red_duration_ticks) {
                    current_auto_state = STATE_GREEN;
                    current_state_timer_ticks = 0; // Reset timer for the new state
                }
            }
            else if (current_auto_state == STATE_GREEN) {
                RED_LIGHT    = 0;
                YELLOW_LIGHT = 0;
                GREEN_LIGHT  = 1;
                if (current_state_timer_ticks >= green_duration_ticks) {
                    current_auto_state = STATE_YELLOW;
                    current_state_timer_ticks = 0;
                }
            }
            else if (current_auto_state == STATE_YELLOW) {
                RED_LIGHT    = 0;
                YELLOW_LIGHT = 1;
                GREEN_LIGHT  = 0;
                if (current_state_timer_ticks >= yellow_duration_ticks) {
                    current_auto_state = STATE_RED;
                    current_state_timer_ticks = 0;
                }
            }
        }

        // This delay determines the basic "tick" rate of the software timer
        // for the automatic sequence and how often the manual switches are polled.
        delay_ms(100); // Makes each tick of current_state_timer_ticks approximately 100ms.
                       // If manual buttons are held, this provides some responsiveness.
    }
}
