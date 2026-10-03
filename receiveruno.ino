#include <SoftwareSerial.h>
#include <EEPROM.h>

SoftwareSerial rfSerial(8, 9); // RX di Pin 8, TX di Pin 9 (tidak dipakai)

#define EXPECTED_DEV_HI 0x00
#define EXPECTED_DEV_LO 0x00

uint16_t last_counter = 0;

void setup() {
  Serial.begin(9600);
  rfSerial.begin(1200); // Harus 1200 bps!
  
  pinMode(2, OUTPUT); pinMode(3, OUTPUT);
  pinMode(4, OUTPUT); pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);

  // Muat counter terakhir dari EEPROM (Alamat 0 dan 1)
  last_counter = (EEPROM.read(0) << 8) | EEPROM.read(1);
  Serial.println("System Ready. Waiting for secure RF packets...");
}

void loop() {
  if (rfSerial.available() >= 6) {
    if (rfSerial.read() == 0xFF) { // Cari Header Paket
      
      uint8_t dev_hi = rfSerial.read();
      uint8_t dev_lo = rfSerial.read();
      uint8_t cnt_hi = rfSerial.read();
      uint8_t cnt_lo = rfSerial.read();
      uint8_t buttons = rfSerial.read();
      uint8_t checksum = rfSerial.read();

      // 1. Verifikasi Checksum dan Device ID
      uint8_t calc_check = dev_hi ^ dev_lo ^ cnt_hi ^ cnt_lo ^ buttons;
      
      if (calc_check == checksum && dev_hi == EXPECTED_DEV_HI && dev_lo == EXPECTED_DEV_LO) {
        
        // 2. Verifikasi Rolling Counter (Anti-Replay Attack)
        uint16_t current_counter = (cnt_hi << 8) | cnt_lo;
        
        if (current_counter > last_counter || (last_counter > 65000 && current_counter < 100)) {
          last_counter = current_counter;
          
          // Simpan counter baru ke EEPROM
          EEPROM.update(0, cnt_hi);
          EEPROM.update(1, cnt_lo);

          Serial.print("Auth Success! Buttons: ");
          Serial.println(buttons, BIN);

          // 3. Eksekusi Output (Active LOW transmisi)
          digitalWrite(2, !(buttons & (1 << 0)) ? HIGH : LOW); // RA0
          digitalWrite(3, !(buttons & (1 << 1)) ? HIGH : LOW); // RA1
          digitalWrite(4, !(buttons & (1 << 2)) ? HIGH : LOW); // RA2
          digitalWrite(5, !(buttons & (1 << 4)) ? HIGH : LOW); // RA4
          digitalWrite(6, !(buttons & (1 << 5)) ? HIGH : LOW); // RA5
          
          delay(100); // Tahan output sebentar
        } else {
          Serial.println("WARNING: Replay Attack Detected! (Counter invalid)");
        }
      }
      
      // Matikan semua output jika tidak ada sinyal
      digitalWrite(2, LOW); digitalWrite(3, LOW);
      digitalWrite(4, LOW); digitalWrite(5, LOW);
      digitalWrite(6, LOW);
    }
  }
}