# Architettura firmware

## Panoramica

Il firmware e' implementato come macchina a stati finiti (`enum Mode`) con loop non bloccante.
Le funzionalita' principali convivono senza `delay()` operativo (tranne splash iniziale), usando `millis()` per refresh display, blink campi e gestione buzzer.

File sorgente:
- `firmware/03-orologio-datario-LCD-I2C/03-orologio-datario-LCD-I2C.ino`

## Moduli logici principali

- **RTC DS3231**
  - lettura data/ora corrente (`rtc.now()`)
  - set ora/data (`rtc.adjust(...)`)
  - gestione sveglia giornaliera con Alarm1

- **Display LCD 16x2 I2C**
  - rendering pagine UI per ogni stato
  - buffering per ridurre sfarfallio (`lcdPrintLineBuffered`)
  - carattere custom campanella in posizione finale riga 2

- **Input encoder + pulsante**
  - quadratura encoder con interrupt `PCINT2_vect` su D3/D4
  - accumulo delta e riduzione rumore tramite `readEncDeltaAtomic()`
  - eventi pulsante: click corto / pressione lunga (`LONG_PRESS_MS = 1000`)

- **Buzzer attivo LOW**
  - beep non bloccante con timeout (`buzzerStart`, `buzzerUpdate`)
  - sequenza sveglia con lampeggio retroilluminazione (`alarmBeepSequenceUpdate`)

- **Persistenza EEPROM**
  - inizializzazione con magic byte (`0xA5`)
  - salvataggio impostazioni utente e sveglia

## Macchina a stati (`Mode`)

Stati definiti:
- `RUN`
- `MENU`
- `SET_TIME`
- `SET_DATE`
- `SET_TIMER`
- `TIMER_RUNNING`
- `PARAM_MENU`
- `ALARM_MENU`
- `SET_ALARM`
- `STOPWATCH_RUNNING`

Approccio:
- ogni stato gestisce input (`step`, `evtShortClick`, `evtLongPress`)
- rendering periodico vincolato da `REFRESH_MS`
- transizioni esplicite tramite funzioni `enter...()`

## Temporizzazioni principali

- `REFRESH_MS = 120` ms: refresh display/UI
- `BLINK_MS = 350` ms: lampeggio campo in edit
- `PARAM_TIMEOUT_MS = 8000` ms: uscita automatica da `PARAM_MENU`
- `LONG_PRESS_MS = 1000` ms: soglia pressione lunga pulsante encoder

## Gestione sveglia DS3231

- Il pin `INT/SQW` del DS3231 e' collegato a `D7` (`ALARM_PIN`).
- In setup:
  - disattivazione square wave
  - pulizia flag allarmi
  - programmazione Alarm1 giornaliera su `alarmHour:alarmMin:00`
- In loop:
  - polling livello pin (`LOW` = evento sveglia)
  - avvio sequenza beep
  - clear flag Alarm1

## Timer countdown

- Set da menu (`HH:MM:SS`, max 99 ore)
- Start solo se durata > 0
- In esecuzione:
  - click corto: pausa/riprendi
  - pressione lunga: stop/uscita
- A fine timer:
  - beep 500 ms
  - messaggio `TIMER SCADUTO` per circa 2 s

## Cronometro

- Base tempo: `millis()`
- Stati logici: fermo / running / pausa
- Click corto: start-pause-resume
- Pressione lunga: reset + uscita a `RUN`
- Formato visualizzato: `MM:SS:CC`

## EEPROM: mappa indirizzi

- `0`: magic byte
- `1`: backlight (`0/1`)
- `2`: allineamento orario (`0..2`)
- `3`: sveglia abilitata (`0/1`)
- `4`: ora sveglia (`0..23`)
- `5`: minuti sveglia (`0..59`)

Nota: la voce `RESET SETTINGS` e' prevista in UI ma la logica e' volutamente non implementata nel firmware attuale.

## Pinout firmware (riepilogo)

- Encoder `CLK` -> `D4`
- Encoder `DT` -> `D3`
- Encoder `SW` -> `D2`
- Buzzer -> `D6` (attivo LOW)
- DS3231 `INT/SQW` -> `D7`
- I2C -> `A4 SDA`, `A5 SCL`