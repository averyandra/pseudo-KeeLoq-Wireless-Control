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

// Device ID Unik untuk Remote Anda (Ganti angka ini untuk remote lain)
#define DEVICE_ID_HI 0x00
#define DEVICE_ID_LO 0x00

// Fungsi Baca/Tulis EEPROM Bare-metal
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

// Fungsi UART Bit-Banging (1200 bps)
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

// ISR untuk membangunkan IC dari Sleep
void __interrupt() isr(void) {
    if (INTCONbits.RAIF) {
        uint8_t dummy = PORTA; // Bersihkan status mismatch
        INTCONbits.RAIF = 0;
    }
}

void main(void) {
    TRISC = 0b00000000;
    PORTC = 0b00000000; 

    // RA0, RA1, RA2, RA4, RA5 sebagai input tombol
    TRISA = 0b00110111;     
    ANSEL = 0b00000000;     
    
    // Aktifkan Internal Pull-up agar tidak perlu resistor eksternal
    OPTION_REGbits.nRAPU = 0;
    WPUA = 0b00110111;
    
    IOCA = 0b00110111;      // Interrupt-on-change aktif
    INTCONbits.RAIE = 1;
    INTCONbits.GIE = 1;

    while(1) {
        SLEEP(); // IC Mati Suri (Konsumsi arus Nano-Ampere)
        
        __delay_ms(30); // Debounce
        
        uint8_t buttons = PORTA & 0b00110111;
        if (buttons != 0b00110111) { // Jika ada tombol ditekan
            
            // 1. Ambil & Naikkan Rolling Counter dari EEPROM
            uint8_t cnt_hi = eeprom_read_byte(0);
            uint8_t cnt_lo = eeprom_read_byte(1);
            uint16_t counter = (cnt_hi << 8) | cnt_lo;
            counter++;
            eeprom_write_byte(0, (counter >> 8));
            eeprom_write_byte(1, (counter & 0xFF));

            // 2. Hitung Checksum Keamanan
            uint8_t checksum = DEVICE_ID_HI ^ DEVICE_ID_LO ^ cnt_hi ^ cnt_lo ^ buttons;

            // 3. Transmisikan Paket RF berulang selama ditekan
            while ((PORTA & 0b00110111) != 0b00110111) {
                // Preamble & Sync (Membangunkan Receiver AGC)
                send_byte(0xAA); send_byte(0xAA); send_byte(0xAA);
                send_byte(0xFF); // Header Paket
                
                // Payload
                send_byte(DEVICE_ID_HI);
                send_byte(DEVICE_ID_LO);
                send_byte(cnt_hi);
                send_byte(cnt_lo);
                send_byte(buttons);
                send_byte(checksum);
                
                __delay_ms(100); 
            }
            RF_DATA = 0; // Matikan Tx
        }
    }
}