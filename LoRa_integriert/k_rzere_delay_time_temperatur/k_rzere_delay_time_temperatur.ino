// Feather M0 LoRa – Low-Power mit TPL5111, frische MCP9600-Werte
#include <Wire.h>                          // I2C-Bibliothek für Sensor-Kommunikation
#include <SPI.h>                           // SPI-Bibliothek für LoRa-Transceiver
#include <RH_RF95.h>                       // RadioHead-Treiber für RFM95 (LoRa)
#include <Adafruit_MCP9600.h>              // Adafruit-Treiber für den Thermocouple-Verstärker MCP9600

//Sender-ID
#define ID 1

// ======== LoRa / Funk =========
#define RF95_FREQ 868.0                    // LoRa-Frequenz für EU-Band (868 MHz)
#define RF_TX_DBM 10                       // Sendeleistung in dBm (max. 20, abhängig von Board/Region)
#define I2C_SPEED 100000L                  // I2C-Taktfrequenz: 100 kHz (Standard)

// ======== Busy-Detection (CCA) ========
#define CCA_RSSI_THRESH_DBM -90    // ueber dieser Schwelle gilt der Kanal als belegt -- KALIBRIEREN!
#define CCA_BACKOFF_MS      100    // einmaliges Warten, wenn der Kanal belegt ist

// ======== Pins am Feather M0 ========
#define RFM95_CS   8                      // Chip-Select-Pin des RFM95 am Feather M0
#define RFM95_RST  4                      // Reset-Pin des RFM95
#define RFM95_INT  3                       // DIO0/INT-Pin des RFM95 (IRQ)
#define SDA_PIN    20                      // I2C SDA-Pin (Feather M0)
#define SCL_PIN    21                      // I2C SCL-Pin (Feather M0)

// ======== TPL5111 DONE-Pin ========
// Diesen Pin vom Feather mit dem DONE-Pin des TPL5111 verbinden
#define TPL_DONE_PIN 5                     // Wähle einen freien Digital-Pin (z.B. D5)

// Objekte
RH_RF95 rf95(RFM95_CS, RFM95_INT);         // RadioHead-Objekt mit CS- und IRQ-Pin
Adafruit_MCP9600 mcp;                      // MCP9600-Sensorobjekt
struct __attribute__((packed)) TempPayload { //Objekt um Daten kompakt zu versenden
  uint8_t id;
  int16_t th;   
};

// ---------- RFM95 Hardware-Reset ----------
static void loraReset() {                  // Hardware-Reset-Sequenz für das RFM95
  pinMode(RFM95_RST, OUTPUT);              // RST-Pin als Ausgang
  digitalWrite(RFM95_RST, HIGH); delay(10); // kurz auf HIGH (Pull-up stabilisieren)
  digitalWrite(RFM95_RST, LOW);  delay(10); // LOW-Impuls zum Reset auslösen
  digitalWrite(RFM95_RST, HIGH); delay(10);// wieder HIGH und kurze Wartezeit
}

// ---------- Frische MCP9600-Werte lesen ----------
static void readFresh(double &th, double &tc) {  // th=Thermocouple, tc=Ambient
  mcp.enable(true);                        // MCP9600 aktivieren (Messung einschalten)
  (void)mcp.readThermocouple();            // Dummy-Read: FIFO/Filter „spülen“
  (void)mcp.readAmbient();                 // Dummy-Read: Umgebungstemperatur „spülen“
  delay(90);                               // Wartezeit, damit neue Konversionen fertig sind
  th = mcp.readThermocouple();             // Thermoelement-Temperatur (Heißstelle) lesen
  tc = mcp.readAmbient();                  // Umgebungssensor (Kaltstelle) lesen
                                           // (MCP9600 führt intern die Thermospannungsberechnung durch)
}

// ---------- Aktueller Kanal-RSSI aus Register 0x1B (in dBm) ----------
static int16_t chanRssiDbm() {
  // 0x1B = momentaner Pegel im Kanal (NICHT der des letzten Pakets!)
  return -157 + (int16_t)rf95.spiRead(RH_RF95_REG_1B_RSSI_VALUE);
}

// ---------- Kurz zuhoeren: sendet gerade jemand? ----------
static bool channelBusy() {
  rf95.setModeRx();                   // Empfaenger einschalten
  delay(2);                           // kurz einschwingen lassen (PLL/AGC)
  int16_t peak = -200;                // hoechsten Pegel im Lauschfenster suchen
  for (uint8_t i = 0; i < 10; i++) {  // ~2,5 ms lauschen
    int16_t r = chanRssiDbm();
    if (r > peak) peak = r;
    delayMicroseconds(250);
  }
  rf95.setModeIdle();                 // Empfaenger wieder in Standby
  return (peak > CCA_RSSI_THRESH_DBM);
}

// ---------- Einmaliger Mess- und Sendezyklus ----------
static void doMeasurementAndSend() {
  double th, tc;
  readFresh(th, tc);                       // Frische Sensorwerte holen
  
  //Payload formatieren
  TempPayload payload;
  payload.id = (uint8_t)ID;
  payload.th = (int16_t)round(th * 10.0f);
  
  // --------- Busy-Detection: kurz hineinhoeren ----------
  if (channelBusy()) {                // Kanal belegt?
    rf95.sleep();                     // RFM95 stromsparend abschalten
    delay(CCA_BACKOFF_MS);            // einmal 100 ms warten
  }
  
  rf95.setModeIdle();                               // Transceiver auf Idle (bereit zum Senden)
  rf95.send((uint8_t*)&payload, sizeof(payload));   // Nachricht als Bytes versenden
  rf95.waitPacketSent();                            // Blockieren, bis Paket vollständig gesendet
  rf95.sleep();                                     // Danach wieder in Sleep (Strom sparen)
}

// =====================================================
// setup(): Wird nach jedem Einschalten durch TPL5111 einmal aufgerufen
// =====================================================
void setup() {
  // --------- TPL5111 DONE-Pin zuerst setzen ---------
  pinMode(TPL_DONE_PIN, OUTPUT);
  digitalWrite(TPL_DONE_PIN, LOW);         // LOW halten, damit TPL5111 eingeschaltet lässt

  // --------- I2C / MCP9600 initialisieren ---------
  Wire.begin(SDA_PIN, SCL_PIN);            // I2C mit expliziten Pins starten
  Wire.setClock(I2C_SPEED);                // I2C-Geschwindigkeit setzen (100 kHz)
  mcp.begin(0x67, &Wire);                  // MCP9600 am I2C-Addr 0x67 initialisieren (ggf. 0x66)
  mcp.setThermocoupleType(MCP9600_TYPE_K);// Thermoelement-Typ konfigurieren (hier: Typ K)
  mcp.setFilterCoefficient(3);             // Digitalfilter (0–7): Glättung vs. Reaktionszeit
  mcp.setADCresolution(MCP9600_ADCRESOLUTION_14); // 14-bit ADC-Auflösung (0,25°C)
  mcp.setAmbientResolution(RES_ZERO_POINT_25);//setze Ambient Resolution auf 0,25°C

  // --------- LoRa / RFM95 initialisieren ---------
  SPI.begin();                             // SPI-Bus für LoRa starten
  loraReset();                             // RFM95 Hardware-Reset ausführen
  rf95.init();                             // RadioHead init (SPI + Register prüfen)
  rf95.setFrequency(RF95_FREQ);            // Arbeitsfrequenz einstellen (868 MHz)
  rf95.setTxPower(RF_TX_DBM, false);       // Sendeleistung setzen, PA_BOOST=false (je nach Board)
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // Modemparameter (BW=125kHz, CR=4/5, SF=128=SF7)
  rf95.sleep();                            // Transceiver erst mal in Sleep

  // --------- EINMALIG: Messen + Senden ----------
  doMeasurementAndSend();

  // --------- TPL5111 informieren: Aufgabe erledigt ----------
  // Wenn DONE von LOW -> HIGH geht, darf TPL5111 den Ausgang abschalten.
  // Wir toggeln DONE in einer Endlosschleife, damit die Flanke sicher erkannt wird.
  while (1) {
    digitalWrite(TPL_DONE_PIN, HIGH);
    delay(2);
    digitalWrite(TPL_DONE_PIN, LOW);
    delay(2);
  }
  // Beim nächsten Timer-Intervall schaltet der TPL5111 wieder ein -> setup() läuft erneut.
}

// loop() bleibt leer, weil alles in setup() erledigt wird
void loop() {
  // Nichts zu tun – der TPL5111 übernimmt das Intervall-Timing
}
