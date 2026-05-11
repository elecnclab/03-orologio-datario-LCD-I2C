# 03-orologio-datario-LCD-I2C

Orologio digitale multifunzione con datario, timer countdown, cronometro e sveglia giornaliera.
Il progetto usa `Arduino Nano`, `RTC DS3231`, `LCD 16x2 I2C`, encoder rotativo (`KY-040`) e buzzer attivo LOW.

Video demo: https://youtu.be/ws_6DP3VXj8

## Caratteristiche principali

- Visualizzazione ora (`HH:MM:SS`) con allineamento configurabile (`SX`, `CTR`, `DX`)
- Datario con giorno settimana in italiano (`dom`...`sab`)
- Impostazione ora e data da menu
- Timer countdown (`00:00:00` a `99:59:59`) con pausa/riprendi
- Cronometro con visualizzazione `MM:SS:CC`
- Sveglia giornaliera basata su interrupt hardware DS3231 (pin `INT/SQW`)
- Parametri persistenti in EEPROM:
  - retroilluminazione LCD
  - allineamento ora
  - stato sveglia e orario sveglia

## Hardware

- Arduino Nano (ATmega328P)
- Modulo RTC DS3231
- Display LCD 16x2 con backpack I2C (indirizzo firmware: `0x27`)
- Encoder rotativo KY-040 con pulsante integrato
- Buzzer attivo LOW

## Collegamenti

### Encoder KY-040

- `CLK` -> `D4`
- `DT` -> `D3`
- `SW` -> `D2`
- `+` -> `5V`
- `GND` -> `GND`

### Bus I2C (LCD + DS3231)

- `SDA` -> `A4`
- `SCL` -> `A5`
- `VCC` -> `5V`
- `GND` -> `GND`

### Buzzer attivo LOW

- `IN` -> `D6`
- `GND` -> `GND`

### Interrupt sveglia DS3231

- `INT/SQW` -> `D7`

## Librerie Arduino usate

- `Wire.h`
- `RTClib.h`
- `LiquidCrystal_I2C.h`
- `EEPROM.h`

## Struttura repository

- `.github/`
- `firmware/03-orologio-datario-LCD-I2C/03-orologio-datario-LCD-I2C.ino`
- `docs/bom/`
- `docs/schemi/`
- `docs/note-tecniche/`
- `media/foto/`
- `media/github-images/`
- `hardware/stl/`
- `CHANGELOG.md`
- `VERSION`

## Media

- Foto progetto: `media/foto/miniatura-1.png`, `media/foto/miniatura-2.png`
- Immagini per README/GitHub: `media/github-images/`

## File STL

- `hardware/stl/base-orologio.stl`
- `hardware/stl/front-orologio.stl`

## Compilazione e upload

1. Apri lo sketch in Arduino IDE:
   - `firmware/03-orologio-datario-LCD-I2C/03-orologio-datario-LCD-I2C.ino`
2. Verifica/installa le librerie richieste.
3. Se necessario, adatta:
   - indirizzo I2C LCD (`LCD_ADDR`, default `0x27`)
   - verso encoder (`ENCODER_DIRECTION`, default `+1`)
4. Seleziona scheda e porta corrette.
5. Compila e carica.

## Uso rapido interfaccia

- **RUN (schermata normale)**
  - Rotazione encoder: apre `PARAM/INFO`
  - Pressione lunga: apre `MENU` principale

- **MENU principale**
  - Voci: `ORA`, `DATA`, `TIMER`, `SVEGLIA`, `CRONOMETRO`
  - Rotazione: cambia voce
  - Click: entra/modifica
  - Pressione lunga: ritorna a `RUN`

Per il dettaglio completo della logica stati e comandi:
- `docs/note-tecniche/architettura-firmware.md`
- `docs/note-tecniche/mappa-menu-e-comandi.md`

## Note operative

- Alla prima accensione, se RTC ha perso alimentazione, l'orario viene impostato alla data/ora di compilazione firmware.
- La riga data su LCD mostra 15 caratteri + icona campanella (se sveglia attiva) nell'ultimo carattere.
- Nel menu `PARAM/INFO` la voce `RESET SETTINGS` e' presente ma non implementata (volutamente lasciata vuota nel firmware attuale).

## Stato progetto

Firmware funzionante e testato fisicamente su hardware reale.
Documentazione tecnica in aggiornamento incrementale in `docs/note-tecniche/`.
