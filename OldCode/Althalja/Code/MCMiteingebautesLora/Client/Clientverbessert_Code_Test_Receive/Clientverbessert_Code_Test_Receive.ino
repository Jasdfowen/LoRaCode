// Feather M0 LoRa – Low-Power mit TPL5111, frische MCP9600-Werte
#include <Wire.h>                          // I2C-Bibliothek für Sensor-Kommunikation
#include <SPI.h>                           // SPI-Bibliothek für LoRa-Transceiver
#include <RH_RF95.h>                       // RadioHead-Treiber für RFM95 (LoRa)
#include <Adafruit_MCP9600.h>              // Adafruit-Treiber für den Thermocouple-Verstärker MCP9600

// ======== LoRa / Funk =========
#define RF95_FREQ 868.0                    // LoRa-Frequenz für EU-Band (868 MHz)
#define RF_TX_DBM 10                       // Sendeleistung in dBm (max. 20, abhängig von Board/Region)
#define I2C_SPEED 100000L                  // I2C-Taktfrequenz: 100 kHz (Standard)

// ======== Pins am Feather M0 ========
#define RFM95_CS   10                      // Chip-Select-Pin des RFM95 am Feather M0
#define RFM95_RST  11                      // Reset-Pin des RFM95
#define RFM95_INT  6                       // DIO0/INT-Pin des RFM95 (IRQ)
#define SDA_PIN    20                      // I2C SDA-Pin (Feather M0)
#define SCL_PIN    21                      // I2C SCL-Pin (Feather M0)

// ======== TPL5111 DONE-Pin ========
// Diesen Pin vom Feather mit dem DONE-Pin des TPL5111 verbinden
#define TPL_DONE_PIN 5                     // Wähle einen freien Digital-Pin (z.B. D5)

// Objekte
RH_RF95 rf95(RFM95_CS, RFM95_INT);         // RadioHead-Objekt mit CS- und IRQ-Pin
Adafruit_MCP9600 mcp;                      // MCP9600-Sensorobjekt

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
  delay(300);                              // Wartezeit, damit neue Konversionen fertig sind
  th = mcp.readThermocouple();             // Thermoelement-Temperatur (Heißstelle) lesen
  tc = mcp.readAmbient();                  // Umgebungssensor (Kaltstelle) lesen
                                           // (MCP9600 führt intern die Thermospannungsberechnung durch)
}

// ---------- Einmaliger Mess- und Sendezyklus ----------
static void doMeasurementAndSend() {
  double th, tc;
  readFresh(th, tc);                       // Frische Sensorwerte holen
  double dT = th - tc;                     // Temperaturdifferenz (Heißstelle - Umgebung)

  char msg[40];                            // Puffer für zu sendende Nachricht
  snprintf(msg, sizeof(msg), "ID:1: %.1f,%.1f,%.1f", th, tc, dT); // CSV-Format mit 1 Nachkommastelle

  rf95.setModeIdle();                      // Transceiver auf Idle (bereit zum Senden)
  rf95.send((uint8_t*)msg, strlen(msg));   // Nachricht als Bytes versenden
  rf95.waitPacketSent();                   // Blockieren, bis Paket vollständig gesendet
  rf95.sleep();                            // Danach wieder in Sleep (Strom sparen)
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
  mcp.setThermocoupleType(MCP9600_TYPE_J);// Thermoelement-Typ konfigurieren (hier: Typ K)
  mcp.setFilterCoefficient(3);             // Digitalfilter (0–7): Glättung vs. Reaktionszeit
  mcp.setADCresolution(MCP9600_ADCRESOLUTION_18); // Höchste ADC-Auflösung (langsamer, aber präziser)
  
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
  //---------- Wait and turn rx on for 10 ms --------
  rf95.setModeRx();          // Empfänger einschalten (continuous RX)
  delay(2);                 // 10 ms RX-Fenster (nur für Strommessung)
  rf95.sleep();              // Transceiver wieder in Sleep
  // --------- TPL5111 informieren: Aufgabe erledigt ----------
  // Wenn DONE von LOW -> HIGH geht, darf TPL5111 den Ausgang abschalten.
  // Wir toggeln DONE in einer Endlosschleife, damit die Flanke sicher erkannt wird.
  while (1) {
    digitalWrite(TPL_DONE_PIN, HIGH);
    delay(2);
    digitalWrite(TPL_DONE_PIN, LOW);
    delay(2);
  }

  // Danach wird die Versorgung vom TPL5111 getrennt.
  // Beim nächsten Timer-Intervall schaltet der TPL5111 wieder ein -> setup() läuft erneut.
}

// loop() bleibt leer, weil alles in setup() erledigt wird
void loop() {
  // Nichts zu tun – der TPL5111 übernimmt das Intervall-Timing
}
