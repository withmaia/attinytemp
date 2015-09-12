#define F_CPU 1000000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "lib/dht.h"
#include "lib/softuart.h"

#define DHT_DDR DDRB
#define DHT_PORT PORTB
#define DHT_INPUTPIN PB3

#define ESP_PIN 2
#define DHT_PIN 4

// Helpers
// -----------------------------------------------------------------------------

// Delay for some seconds (because _delay* fns only take constant arguments)
void delay_s(int s) {
    for (int i = 0; i<s; i++) {
        _delay_ms(1000);
    }
}

// Celcius to Fahrenheit
float ctof(float c) {
    return c * 1.8 + 32.0;
}

// Float to string
char *floatString(char *str, float n) {
    int d1 = n;            // Get the integer part (678).
    float f2 = n - d1;     // Get fractional part (678.0123 - 678 = 0.0123).
    int d2 = trunc(f2 * 10000);   // Turn into integer (123).
    sprintf(str, "%d.%d", d1, d2);
    return str;
}

// Read temperature from DHT and write to a string to send to ESP
// -----------------------------------------------------------------------------

void show_temp(char *temp_text) {

    float temp = 0;
    float hum = 0;
    int read = dht_gettemperaturehumidity(&temp, &hum);

    if (read == -1) {
        sprintf(temp_text, "READ FAILED");
        return;
    }

    char temps[10];
    char hums[10];
    floatString(temps, ctof(temp));
    floatString(hums, hum);
    sprintf(temp_text, "TEMP %s\rHUM %s\r", temps, hums);
}

// Turn on softuart, send, and turn back off again (to avoid interfering with DHT timing)
// -----------------------------------------------------------------------------

void temp_softuart_puts(char *s) {

    // Turn on ESP and wait for it to hopefully connect
    PORTB |= (1 << ESP_PIN);

    // Turn on UART
    softuart_init();
    softuart_turn_rx_off();
    sei(); 

    delay_s(10);

    //_delay_ms(100);
    softuart_puts(s); // Send reading to ESP
    _delay_ms(100);

    // Turn off UART
    cli();

    // Give ESP some time to send and then turn off
    delay_s(15);
    PORTB &= ~(1 << ESP_PIN);
}

int main(void) {

    DDRB = (1 << ESP_PIN) | (1 << DHT_PIN); // ESP and DHT enable pins
    PORTB = 0; // Turn off everything
    PORTB |= (1 << DHT_PIN); // Turn on DHT
    //PORTB |= (1 << ESP_PIN); // Turn on ESP

    // Intro to clear buffer
    temp_softuart_puts("\rATTINYTEMP STARTING\r");

    delay_s(10); // Give everything 20s to start up

    while (1) {
        // Read from DHT
        char temp_text[50];
        show_temp(&temp_text);

        temp_softuart_puts(&temp_text);

        delay_s(60*10); // Pause until next reading (10 min)
    }

    return 0;
}

