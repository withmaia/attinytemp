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

void setup_softuart() {
    softuart_init();
    sei();
}

char do_read_dht = 0;

int main(void) {

    DDRB = (1 << ESP_PIN) | (1 << DHT_PIN); // ESP and DHT enable pins
    PORTB |= (1 << ESP_PIN); // Turn on ESP

    // Setup UART and write intro
    setup_softuart();
    softuart_puts("\rATTINYTEMP STARTING\r");

    // Set up timer for DHT read
    TCCR1 = (1 << CTC1); // Clear timer on compare match 1C
    TCCR1 |= (1 << CS13) | (1 << CS11) | (1 << CS10); // Clock divide by 1024
    OCR1C = 244; // Compare match 1C value, same as above
    TIMSK |= (1 << OCIE1A); // Enable timer 0 compare match

    // Store incoming lines for parsing
    char read_line[32];
    char ci = 0;

    for (;;) {
        // Handle incoming UART
        while (softuart_kbhit()) {
            char c = softuart_getchar();
            if (c == 'U') {
                softuart_puts("red\r\n");
            } else if (c == 'A') {
                softuart_puts("yellow\r\n");
            } else if (c == 'C') {
                softuart_puts("green\r\n");
            }
        }
        // Read DHT if triggered
        if (do_read_dht) read_dht();
        else _delay_ms(10);
    }
}

void read_dht() {

    PORTB |= (1 << DHT_PIN);
    delay_s(1);

    // Read from DHT
    char temp_text[50];
    show_temp(&temp_text);

    softuart_puts(&temp_text);

    do_read_dht = 0;
}

// "Post scale" of clock, multiplied by number of seconds between each toggle
// = 8mhz / 1024 (prescale) / 244 (timer compare) * [Ns]
#define POSTSCALE1 32 * 10
int ti1 = 0; // Clock counter to reach postscale value

// Compare match 1 routine
ISR(TIM1_COMPA_vect) {
    if (++ti1 == POSTSCALE1) {
        do_read_dht = 1; // Set DHT read trigger
        ti1 = 0;
    }
}
