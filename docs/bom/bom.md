# BOM (Bill of Materials)

Elenco componenti principali del progetto `03-orologio-datario-LCD-I2C`.

## Elettronica

| Qta | Componente | Note |
|---|---|---|
| 1 | Arduino Nano (ATmega328P) | Compatibile 5V |
| 1 | RTC DS3231 | Modulo I2C |
| 1 | LCD 16x2 I2C | Backpack tipico PCF8574, indirizzo firmware `0x27` |
| 1 | Encoder KY-040 | Con pulsante integrato |
| 1 | Buzzer attivo LOW | Pilotato da `D6` |

## Cablaggio rapido

- I2C: `A4` (SDA), `A5` (SCL)
- Encoder: `D4` (CLK), `D3` (DT), `D2` (SW)
- Sveglia DS3231: `INT/SQW` su `D7`
- Buzzer: `IN` su `D6`

