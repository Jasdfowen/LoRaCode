// Feather M0 + RFM95 — Empfangspegel-Messung / Noise-Survey
// Ausgabe im Format für den Arduino Serial Plotter
// (Werkzeuge -> Serieller Plotter, 115200 Baud).
#include <SPI.h>
#include <RH_RF95.h>

// ---- Pins: Feather M0 + LoRa-Wing (wie in deinem RX-Sketch) ----
#define RFM95_RST 11
#define RFM95_CS  10
#define RFM95_INT  6
#define RF95_FREQ 868.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);

// ---- Survey-Parameter ----
const uint32_t TICK_MS = 20;          // Ausgaberate (5 Hz)
uint32_t lastTick = 0;

// Gehaltene Paketwerte, damit die Plotter-Serien durchgehend bleiben
int16_t pktRssi = -157;                // letzter Paket-RSSI (dBm)
int8_t  pktSnr  = 0;                   // letzter Paket-SNR (dB)

// Aktueller Kanal-RSSI aus Register 0x1B (Datenblatt-Offset HF-Port @868)
int16_t noiseFloorDbm() {
  return -157 + (int16_t)rf95.spiRead(RH_RF95_REG_1B_RSSI_VALUE);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(115200);
  while (!Serial) delay(1);

  digitalWrite(RFM95_RST, LOW);  delay(10);   // manueller Reset
  digitalWrite(RFM95_RST, HIGH); delay(10);

  while (!rf95.init()) { Serial.println("LoRa init failed"); delay(1000); }
  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(23, false);
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);  // MUSS zum Sender passen!

  rf95.setModeRx();   // dauerhaft RX -- sonst liefert Reg 0x1B keinen gültigen Wert
}

void loop() {
  // 1) Eingehendes Paket -> Empfangsstärke + SNR merken
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      pktRssi = rf95.lastRssi();      // RadioHead rechnet bereits in dBm um
      pktSnr  = rf95.lastSNR();
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }

  // 2) Im festen Takt eine Zeile für den Plotter ausgeben
  if (millis() - lastTick >= TICK_MS) {
    lastTick = millis();
    Serial.print("Noise:");    Serial.print(noiseFloorDbm());
    Serial.print(",PktRSSI:"); Serial.print(pktRssi);
    Serial.print(",SNR:");     Serial.println(pktSnr);
    digitalWrite(LED_BUILTIN, LOW);
  }
}
