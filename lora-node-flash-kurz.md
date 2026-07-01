# LoRa-Node flashen — Kurzanleitung

Firmware auf den **Feather M0 (SAMD21G18A)** flashen — bare-metal ohne Bootloader,
ab Adresse `0x00000000`, mit dem **Pi 5 als SWD-Programmer**.

## 1. Verkabeln (Pi 5 → Feather)

| Pi 5 | Signal | Feather |
|------|--------|---------|
| GPIO24 | SWDIO | SWDIO |
| GPIO25 | SWCLK | SWCLK |
| GPIO18 | RESET | RESET |
| GND | GND | GND |

## 2. Bauen (arduino-cli)

`-DCRYSTALLESS` **an den kompletten** `build.extra_flags`-String **anhängen**
(nicht überschreiben — sonst fehlen die Device-Defines und der Build bricht).
Der Flag spart den ~2-s-Quarz-Stall bei jedem Cold Boot.

```bash
arduino-cli compile --clean \
  --fqbn adafruit:samd:adafruit_feather_m0 \
  --build-property "build.extra_flags=<ORIGINAL-STRING> -DCRYSTALLESS" \
  --output-dir ./build ./LoRaNode.ino
```

Linkerskript `flash_without_bootloader.ld` muss eingebunden sein (Start auf `0x0000`).

## 3. Flashen (OpenOCD)

Reihenfolge zwingend: freigeben → anhalten → löschen → schreiben.

```bash
openocd -f feather_swd.cfg \
  -c init \
  -c "at91samd bootloader 0" \
  -c "reset halt" \
  -c "at91samd chip-erase" \
  -c "program build/LoRaNode.ino.elf verify 0x00000000" \
  -c "reset run" -c shutdown
```

- `at91samd bootloader 0` → gibt die geschützte Region ab `0x0000` frei
- `reset halt` → Core anhalten (**Pflicht**, sonst schlägt das Schreiben fehl)
- `at91samd chip-erase` → Flash komplett löschen
- `program … 0x00000000` → schreiben + verifizieren

## 4. Prüfen

`** Verified OK **` in der OpenOCD-Ausgabe, und aktive Phase wieder ~300 ms
(per `millis()` im Payload / Oszilloskop). Ist der ~2-s-Stall noch da →
`-DCRYSTALLESS` hat nicht gegriffen.
