#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_RST 11
#define RFM95_CS  10
#define RFM95_INT  6
#define RF95_FREQ 868.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(115200);
  while (!Serial) delay(1);

  digitalWrite(RFM95_RST, LOW);  delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);

  while (!rf95.init()) { Serial.println("LoRa init failed"); while(1); }
  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(23, false);
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);  // gleiche Config wie Sender

  Serial.println("Dauersenden gestartet...");
}

void loop() {
  uint8_t msg[] = "BLOCK";
  rf95.send(msg, sizeof(msg) - 1);
  rf95.waitPacketSent();
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // blinkt bei jeder Sendung
}
