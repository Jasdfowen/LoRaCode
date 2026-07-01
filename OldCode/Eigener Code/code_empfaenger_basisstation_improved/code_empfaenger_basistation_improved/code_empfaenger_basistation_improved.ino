#include <SPI.h>
#include <RH_RF95.h>

// Feather m0 w/wing:
#define RFM95_RST 11  // "A"
#define RFM95_CS  10  // "B"
#define RFM95_INT  6  // "D"


// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 868.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);
unsigned long lastSend = 0;
struct __attribute__((packed)) TempPayload { //Objekt um Daten kompakt zu versenden
  uint8_t id;
  int16_t th;   
};

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(115200);
  while (!Serial) delay(1);
  delay(100);

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  
  Serial.print("Start Receive: ");

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }

  // Defaults after init are 868.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  //Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  // Defaults after init are 868.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);
}

void loop() {
  if (rf95.available()) {

    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN + 1];
    uint8_t len = RH_RF95_MAX_MESSAGE_LEN;

    if (rf95.recv(buf, &len)&& len == sizeof(TempPayload)) {

        TempPayload p;
        memcpy(&p, buf, sizeof(p));

        digitalWrite(LED_BUILTIN, HIGH);

        Serial.print("Start Receive: ");
        
        float th = p.th / 10.0;   // 235 → 23.5°C
        int id = (int)p.id;

        Serial.printf("%d,%.1f",id,th);

        Serial.println(" :End Receive");
    }
    else {
        Serial.println("Receive failed");
    }
}
}
