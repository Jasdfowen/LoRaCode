// Heltec ESP32 LoRa – SERVER: empfängt ASCII und zeigt sie nacheinander (Log) auf dem OLED
#include <Wire.h>                          // I2C-Bibliothek (für OLED)
#include <SPI.h>                           // SPI-Bibliothek (für LoRa)
#include <RH_RF95.h>                       // RadioHead-Treiber für RFM95
#include <Adafruit_GFX.h>                  // Grafikgrundlagen für OLED
#include <Adafruit_SSD1306.h>              // Treiber für SSD1306-OLED

// ===== OLED =====
#define SCREEN_WIDTH 128                   // OLED-Breite in Pixel
#define SCREEN_HEIGHT 64                   // OLED-Höhe in Pixel
#define OLED_SDA 4                         // I2C SDA-Pin auf Heltec ESP32
#define OLED_SCL 15                        // I2C SCL-Pin auf Heltec ESP32
#define OLED_RST 16                        // OLED-Reset-Pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // OLED-Objekt über I2C

// ===== LoRa (Heltec V2 Pinout) =====
#define RFM95_CS   18                      // Chip-Select des RFM95
#define RFM95_RST  14                      // Reset-Pin des RFM95
#define RFM95_INT  26                      // DIO0/IRQ-Pin des RFM95
#define RF95_FREQ  868.0                   // Arbeitsfrequenz (muss zum Sender passen)
#define LORA_SCK    5                      // SPI SCK-Pin
#define LORA_MISO  19                      // SPI MISO-Pin
#define LORA_MOSI  27                      // SPI MOSI-Pin

RH_RF95 rf95(RFM95_CS, RFM95_INT);         // RadioHead-Objekt (CS, IRQ)

// ===== Log-Puffer (zeigt zuletzt empfangene Nachrichten hintereinander) =====
static const uint8_t MAX_LINES = 6;        // Max. 6 Textzeilen ins 64px-Display (mit Header)
String lines[MAX_LINES];                   // Ringpuffer der letzten Zeilen
uint8_t head = 0;                          // Index der nächsten zu überschreibenden Zeile
uint32_t lastDraw = 0;                     // Zeitstempel der letzten OLED-Aktualisierung

// --- OLED Kopf ---
void drawHeader(const char* status){       // Zeichnet Kopfzeile mit Status-Text
  display.clearDisplay();                  // Bildschirm löschen
  display.setTextSize(1);                  // Textgröße 1 (5x7)
  display.setTextColor(SSD1306_WHITE);     // Farbe: Weiß (monochrom)
  display.setCursor(0,0);                  // Cursor oben links
  display.print("LoRa 868  ["); display.print(status); display.println("]"); // Titel + Status
  display.drawLine(0,10,SCREEN_WIDTH,10,SSD1306_WHITE); // Trennlinie unter Kopf
}

// --- Log-Rendering ---
void redrawLog(){                          // Zeichnet alle gespeicherten Zeilen neu
  drawHeader("RX-Log");                    // Kopf „RX-Log“ anzeigen
  // Startposition für Textzeilen
  int y = 14;                              // Erste Textzeile unter der Trennlinie
  display.setTextSize(1);                  // Textgröße 1

  // Älteste zuerst zeichnen
  for(uint8_t i=0;i<MAX_LINES;i++){        // Schleife über alle Slots im Ringpuffer
    uint8_t idx = (head + i) % MAX_LINES;  // Index in chronologischer Reihenfolge
    if(lines[idx].length() == 0) continue; // Leere Einträge überspringen
    display.setCursor(0, y);               // Cursor setzen
    // Auf ~21-22 Zeichen begrenzen, damit es in 128px passt
    String s = lines[idx];                 // Zeile kopieren
    if(s.length() > 22) s = s.substring(0, 22); // Hartes Kürzen für Displaybreite
    display.print(s);                      // Zeile ausgeben
    y += 8;                                // Nächste Zeile: 8px tiefer
  }
  display.display();                       // Puffer auf Display übertragen
  lastDraw = millis();                     // Zeitstempel aktualisieren
}

// --- Neue Zeile in den Ringpuffer einfügen ---
void pushLine(const String& s){            // Fügt neue Zeile ein und zeichnet neu
  // Überschreibe die aktuelle Kopfzeile, dann Kopf weiterschieben
  lines[head] = s;                         // Aktuellen Slot befüllen
  head = (head + 1) % MAX_LINES;           // Kopf eins weiter (Ring)
  redrawLog();                             // Anzeige aktualisieren
}

void setup(){
  Serial.begin(115200);                    // Serielle Konsole für Debug
  while(!Serial) {}                        // Warten (bei manchen Boards nötig)

  // OLED init
  Wire.begin(OLED_SDA, OLED_SCL);          // I2C mit spezifischen Pins starten
  pinMode(OLED_RST, OUTPUT);               // OLED-Resetpin als Ausgang
  digitalWrite(OLED_RST, LOW);  delay(50); // Reset-Impuls
  digitalWrite(OLED_RST, HIGH); delay(50); // Release Reset
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){ // OLED init an I2C-Adresse 0x3C
   // Serial.println("OLED nicht gefunden!");     // Optionaler Fehlerhinweis (auskommentiert)
    while(1);                           // Halt, wenn kein Display gefunden
  }
  drawHeader("Init"); display.display();   // Initialen Header anzeigen

  // LoRa init
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, RFM95_CS); // SPI mit benutzerdefinierten Pins
  pinMode(RFM95_RST, OUTPUT);              // RFM95 Reset als Ausgang
  digitalWrite(RFM95_RST, HIGH); delay(10);// Reset-Leitung vorbereiten
  digitalWrite(RFM95_RST, LOW);  delay(10);// Reset auslösen
  digitalWrite(RFM95_RST, HIGH); delay(10);// Reset freigeben

  if(!rf95.init()){ Serial.println("RF95 init Fehler"); while(1); } // Treiber/Hardware prüfen
  if(!rf95.setFrequency(RF95_FREQ)){ Serial.println("Freq Fehler"); while(1); } // Frequenz setzen
  rf95.setTxPower(14, false);               // Sendeleistung (für Antworten/Relays; hier Empfangsgerät)
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // Modemparameter (müssen Sender entsprechen)

  drawHeader("Bereit");                    // Status auf Display
  display.setCursor(0, 14);
  display.println("Warte auf Pakete...");  // Infozeile
  display.display();                        // Anzeigen
  Serial.println("Server bereit, warte auf Pakete..."); // Konsolenhinweis
}

void loop(){
  if(rf95.available()){                    // Prüfen, ob ein Paket im RX-Puffer liegt
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];  // Empfangspuffer
    uint8_t len = sizeof(buf);             // Maximale Länge initial setzen

    if(rf95.recv(buf, &len)){              // Paket empfangen (len wird angepasst)
      if(len >= sizeof(buf)) len = sizeof(buf)-1; // Sicherheitskürzung (Nullterminator)
      buf[len] = 0;                        // C-String terminieren
      char* msg = (char*)buf;              // Zeiger auf empfangenen Text

      int16_t rssi = rf95.lastRssi();      // Empfangspegel (RSSI) auslesen

      // Auf Konsole
      Serial.print("RX ["); Serial.print(len); Serial.print("] "); // Länge ausgeben
      Serial.print(msg);                                         // Inhalt ausgeben
      Serial.print("  RSSI="); Serial.println(rssi);             // Pegel ausgeben

      // Zusammengefasste Logzeile bauen: "RSSI=-xx  <Nachricht>"
      // Kürzen auf ca. 22 Zeichen übernimmt redrawLog()
     // String line = "R=" + String(rssi) + " " + String(msg);   // Alternative mit RSSI (auskommentiert)
     String line = "Temp: " + String(msg); // Anzeigepräfix „Temp:“ + gesamte Nachricht (CSV)
      pushLine(line);                       // In Ringpuffer legen und anzeigen

      
    }
  } else {
    // optional: alle paar Sekunden Kopf neu zeichnen, falls Ghosting
    if (millis() - lastDraw > 5000) redrawLog(); // Refresh nach 5 s
    delay(15);                                   // Kleine Entlastung der CPU
  }
}
