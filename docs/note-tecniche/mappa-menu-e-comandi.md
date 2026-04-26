# Mappa menu e comandi encoder

Questa nota descrive il comportamento dell'interfaccia utente nel firmware attuale.

## Convenzioni input

- **Rotazione encoder**: incrementa/decrementa valore o selezione menu
- **Click corto**: conferma / entra nel campo successivo / toggle funzione
- **Pressione lunga** (~1 s): uscita o ritorno al livello precedente

## Stato RUN

Schermata standard:
- Riga 1: ora (`HH:MM:SS`) con allineamento configurabile
- Riga 2: `giorno dd-mm-yyyy` + campanella se sveglia attiva

Comandi:
- Rotazione -> entra in `PARAM_MENU`
- Pressione lunga -> entra in `MENU`

## MENU principale

Voci in ordine:
1. `ORA`
2. `DATA`
3. `TIMER`
4. `SVEGLIA`
5. `CRONOMETRO`

Comandi:
- Rotazione -> cambia voce
- Click corto -> entra nella voce selezionata
- Pressione lunga -> ritorna a `RUN`

## SET ORA (`SET_TIME`)

Campi editabili in sequenza:
1. ore
2. minuti
3. secondi

Comandi:
- Rotazione -> modifica campo corrente (con wrap)
- Click corto -> campo successivo; sull'ultimo campo salva su RTC e torna a `RUN`
- Pressione lunga -> annulla/uscita a `RUN`

## SET DATA (`SET_DATE`)

Campi editabili in sequenza:
1. giorno
2. mese
3. anno (2000..2099)

Comandi:
- Rotazione -> modifica campo corrente (con wrap)
- Click corto -> campo successivo; sull'ultimo campo salva su RTC e torna a `RUN`
- Pressione lunga -> annulla/uscita a `RUN`

Note:
- I limiti del giorno sono coerenti con mese/anno (anni bisestili inclusi).

## SET TIMER (`SET_TIMER`)

Campi editabili in sequenza:
1. ore (0..99)
2. minuti (0..59)
3. secondi (0..59)

Comandi:
- Rotazione -> modifica campo corrente
- Click corto -> campo successivo; sull'ultimo campo avvia countdown (se diverso da zero)
- Pressione lunga -> uscita a `RUN`

## TIMER in esecuzione (`TIMER_RUNNING`)

Visualizzazione:
- Riga 1: orario corrente
- Riga 2: `TIMER HH:MM:SS` oppure `PAUSA HH:MM:SS`

Comandi:
- Click corto -> pausa/riprendi
- Pressione lunga -> stop timer e ritorno a `RUN`

Fine timer:
- beep breve
- messaggio `TIMER SCADUTO`
- ritorno automatico a `RUN`

## Menu parametri/info (`PARAM_MENU`)

Accesso da `RUN` ruotando l'encoder.
Timeout automatico: circa 8 s senza azioni.

Voci:
1. `BACKLIGHT: ON/OFF` (toggle + salvataggio EEPROM)
2. `ORARIO: SX/CTR/DX` (ciclo + salvataggio EEPROM)
3. `TEST BUZZER` (beep 1 s)
4. `INFO FW` (pagina info)
5. `RESET SETTINGS` (placeholder non attivo)

Comandi:
- Rotazione -> cambia voce
- Click corto -> esegue azione voce
- Pressione lunga -> ritorna a `RUN`

In `INFO FW`:
- Click corto o pressione lunga -> ritorno a lista `PARAM_MENU`

## Menu sveglia (`ALARM_MENU`)

Voci:
1. `ATTIVA: ON/OFF`
2. `ORA: HH:MM`

Comandi:
- Rotazione -> seleziona voce
- Click corto su `ATTIVA` -> toggle + salva EEPROM + riprogramma DS3231
- Click corto su `ORA` -> entra in `SET_ALARM`
- Pressione lunga -> ritorna a `RUN`

## SET SVEGLIA (`SET_ALARM`)

Campi editabili:
1. ore
2. minuti

Comandi:
- Rotazione -> modifica campo
- Click corto -> passa al campo successivo; a fine sequenza salva e torna a `ALARM_MENU`
- Pressione lunga -> ritorna a `ALARM_MENU` senza conferma finale

## CRONOMETRO (`STOPWATCH_RUNNING`)

Visualizzazione:
- Riga 1: orario corrente
- Riga 2: `CRONO MM:SS:CC` oppure `PAUSA MM:SS:CC`

Comandi:
- Click corto -> start/pause/resume
- Pressione lunga -> reset cronometro e ritorno a `RUN`