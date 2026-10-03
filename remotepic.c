#include <xc.h>

#pragma config FOSC = INTOSCIO  
#pragma config WDTE = OFF       
#pragma config PWRTE = ON       
#pragma config MCLRE = ON       
#pragma config BOREN = OFF      
#pragma config CPD = OFF        
#pragma config CP = OFF         

#define _XTAL_FREQ 4000000      
#define RF_DATA PORTCbits.RC0   

// Unique Device ID for your Remote (Change these numbers for other remotes)
#define DEVICE_ID_HI 0x00
#define DEVICE_ID_LO 0x00

// Bare-metal EEPROM Read/Write Functions
void eeprom_write_byte(uint8_t addr, uint8_t data) {
    EEADR = addr;
    EEDAT = data;
    EECON1bits.EEPGD = 0;
    EECON1bits.WREN = 1;
    INTCONbits.GIE = 0;
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;
    while(EECON1bits.WR);
    INTCONbits.GIE = 1;
    EECON1bits.WREN = 0;
}

uint8_t eeprom_read_byte(uint8_t addr) {
    EEADR = addr;
    EECON1bits.EEPGD = 0;
    EECON1bits.RD = 1;
    return EEDAT;
}

// UART Bit-Banging Function (1200 bps)
void send_bit(uint8_t b) {
    RF_DATA = b;
    __delay_us(833);
}

void send_byte(uint8_t c) {
    send_bit(0); // Start bit
    for(int i = 0; i < 8; i++) {
        send_bit((c >> i) & 1);
    }
    send_bit(1); // Stop bit
}

// ISR to wake up the IC from Sleep
void __interrupt() isr(void) {
    if (INTCONbits.RAIF) {
        uint8_t dummy = PORTA; // Clear mismatch status
        INTCONbits.RAIF = 0;
    }
}

void main(void) {
    TRISC = 0b00000000;
    PORTC = 0b00000000; 

    // RA0, RA1, RA2, RA4, RA5 as button inputs
    TRISA = 0b00110111;     
    ANSEL = 0b00000000;     
    
    // Enable Internal Pull-ups so external resistors are not needed
    OPTION_REGbits.nRAPU = 0;
    WPUA = 0b00110111;
    
    IOCA = 0b00110111;      // Interrupt-on-change active
    INTCONbits.RAIE = 1;
    INTCONbits.GIE = 1;

    while(1) {
        SLEEP(); // Deep sleep mode (Nano-Ampere current consumption)
        
        __delay_ms(30); // Debounce
        
        uint8_t buttons = PORTA & 0b00110111;
        if (buttons != 0b00110111) { // If a button is pressed
            
            // 1. Fetch & Increment Rolling Counter from EEPROM
            uint8_t cnt_hi = eeprom_read_byte(0);
            uint8_t cnt_lo = eeprom_read_byte(1);
            uint16_t counter = (cnt_hi << 8) | cnt_lo;
            counter++;
            eeprom_write_byte(0, (counter >> 8));
            eeprom_write_byte(1, (counter & 0xFF));

            // 2. Calculate Security Checksum
            uint8_t checksum = DEVICE_ID_HI ^ DEVICE_ID_LO ^ cnt_hi ^ cnt_lo ^ buttons;

            // 3. Continuously transmit RF packet while the button is held down
            while ((PORTA & 0b00110111) != 0b00110111) {
                // Preamble & Sync (Wakes up Receiver AGC)
                send_byte(0xAA); send_byte(0xAA); send_byte(0xAA);
                send_byte(0xFF); // Packet Header
                
                // Payload
                send_byte(DEVICE_ID_HI);
                send_byte(DEVICE_ID_LO);
                send_byte(cnt_hi);
                send_byte(cnt_lo);
                send_byte(buttons);
                send_byte(checksum);
                
                __delay_ms(100); 
            }
            RF_DATA = 0; // Turn off Transmitter
        }
    }
}