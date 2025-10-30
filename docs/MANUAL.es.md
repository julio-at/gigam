# gigamctl — Manual de Usuario

CLI para gestionar entidades y operaciones básicas de un sistema de apuestas (PoC).  
Incluye alta/listado de entidades, registro de cuotas y apuestas, liquidación de eventos, reportes formateados y evaluación de riesgo.

> **Versión de este manual:** coincide con la integración que añade `--format {table|json|csv}` y `--out <file>` en `report`.

---

## Índice

1. [Requisitos y configuración](#requisitos-y-configuración)
2. [Convenciones generales](#convenciones-generales)
3. [Comandos y subcomandos](#comandos-y-subcomandos)  
   3.1 [sport](#sport)  
   3.2 [league](#league)  
   3.3 [team](#team)  
   3.4 [bookmaker](#bookmaker)  
   3.5 [runner](#runner)  
   3.6 [bettor](#bettor)  
   3.7 [event](#event)  
   3.8 [quote](#quote)  
   3.9 [bet](#bet)  
   3.10 [settle](#settle)  
   3.11 [report](#report)  
   3.12 [risk](#risk)
4. [Códigos de salida](#códigos-de-salida)
5. [Ejemplo de sesión “smoke test”](#ejemplo-de-sesión-smoke-test)

---

## Requisitos y configuración

- **Base de datos MySQL/MariaDB** accesible.  
- Variables de entorno que `gigamctl` lee mediante `db_load_env()` (ajusta a tu entorno):

```bash
export DB_HOST=127.0.0.1
export DB_PORT=3306
export DB_USER=root
export DB_PASS=yourpassword
export DB_NAME=gigam
```

- Compilación (en la raíz del proyecto):

```bash
make clean && make
```

El binario resultante es `./gigamctl`.

---

## Convenciones generales

- Los **montos monetarios** se manejan internamente en **centavos** (p. ej., `2500` = `25.00 USD`) a menos que la consulta o reporte ya devuelva USD redondeado.
- Formato de **fechas**: `YYYY-MM-DD`. Para fechas con hora: `YYYY-MM-DD HH:MM[:SS]`.
- En reportes por rango `--from` / `--to` se aplica: `from <= fecha < (to + 1 día)`.  
  Ej.: `--from 2025-10-01 --to 2025-10-31` cubre *todo* octubre 2025.
- A menos que se indique lo contrario, las **listas** devuelven columnas clave y ordenan por `id` o por relevancia del reporte.

---

## Comandos y subcomandos

### sport
Alta y listado de deportes.

#### `sport create`
**Flags obligatorios**
- `--name <string>`: nombre del deporte.  
**Ejemplo**
```bash
./gigamctl sport create --name Soccer
```

#### `sport list`
Lista todos los deportes.
```bash
./gigamctl sport list
```

---

### league
Alta y listado de ligas.

#### `league create`
**Flags obligatorios**
- `--sport-id <id>`: ID del deporte.
- `--name <string>`: nombre de liga.
```bash
./gigamctl league create --sport-id 1 --name "CR Primera"
```

#### `league list`
**Flags opcionales**
- `--sport-id <id>`: lista solo las ligas del deporte indicado.
```bash
./gigamctl league list
./gigamctl league list --sport-id 1
```

---

### team
Alta y listado de equipos.

#### `team create`
**Flags obligatorios**
- `--league-id <id>`
- `--name <string>`
```bash
./gigamctl team create --league-id 1 --name "Alajuelense"
```

#### `team list`
**Flags opcionales**
- `--league-id <id>`
```bash
./gigamctl team list
./gigamctl team list --league-id 1
```

---

### bookmaker
Alta y listado de casas de apuestas.

#### `bookmaker create`
**Flags**
- `--name <string>` (obligatorio)
- `--currency <ISO>` (opcional, por defecto `USD`)
```bash
./gigamctl bookmaker create --name "DemoBook" --currency USD
```

#### `bookmaker list`
```bash
./gigamctl bookmaker list
```

---

### runner
Administración de runners (operadores).

#### `runner create`
**Flags obligatorios**
- `--bookmaker-id <id>`
- `--user <username>`
- `--name <display_name>`
**Flags opcionales**
- `--default` (marca como default)
- `--scheme net|handle` (por defecto `net`)
- `--rate <0..100>` (por defecto `10.0`)
```bash
./gigamctl runner create --bookmaker-id 1 --user runner1 \
  --name "Runner 1" --default --scheme net --rate 12
```

#### `runner list`
**Flags obligatorios**
- `--bookmaker-id <id>`
```bash
./gigamctl runner list --bookmaker-id 1
```

#### `runner set-default`
**Flags obligatorios**
- `--bookmaker-id <id>`
- `--runner-id <id>`
```bash
./gigamctl runner set-default --bookmaker-id 1 --runner-id 1
```

#### `runner payout`
Registra pagos al runner (comisiones liquidadas).

**Flags obligatorios**
- `--runner-id <id>`
- `--amount-cents <int>`
**Flags opcionales**
- `--note <string>`
- `--from <YYYY-MM-DD>` `--to <YYYY-MM-DD>` (período asociado al pago)
```bash
./gigamctl runner payout --runner-id 1 --amount-cents 500 --note "partial" \
  --from 2025-10-01 --to 2025-10-31
```

---

### bettor
Administración de apostadores.

#### `bettor create`
**Flags obligatorios**
- `--runner-id <id>`
- `--code <string>`
**Flags opcionales**
- `--name <display_name>`
```bash
./gigamctl bettor create --runner-id 1 --code B001 --name "Alice"
```

#### `bettor list`
**Flags obligatorios**
- `--runner-id <id>`
```bash
./gigamctl bettor list --runner-id 1
```

#### `bettor payout`
Registra pagos al apostador.

**Flags obligatorios**
- `--bettor-id <id>`
- `--amount-cents <int>`
**Flags opcionales**
- `--note <string>`
- `--from <YYYY-MM-DD>` `--to <YYYY-MM-DD>`
```bash
./gigamctl bettor payout --bettor-id 1 --amount-cents 1000 --note "settlement" \
  --from 2025-10-01 --to 2025-10-31
```

---

### event
Administración de eventos.

#### `event create`
**Flags obligatorios**
- `--league-id <id>`
- `--starts-at "YYYY-MM-DD HH:MM"`
- `--home-id <team_id>`
- `--away-id <team_id>`
```bash
./gigamctl event create --league-id 1 \
  --starts-at "2025-10-30 20:00" --home-id 1 --away-id 2
```

#### `event list`
```bash
./gigamctl event list
```

#### `event set-score`
Actualiza el marcador; `--final` lo marca como finalizado.

**Flags obligatorios**
- `--event-id <id>`
**Flags opcionales**
- `--home <int>` `--away <int>`
- `--final` (marca estado `final`)
```bash
./gigamctl event set-score --event-id 1 --home 2 --away 1 --final
```

#### `event finalize`
Marca un evento como final.

**Flags obligatorios**
- `--event-id <id>`
```bash
./gigamctl event finalize --event-id 1
```

---

### quote
Gestión de cuotas/cotizaciones.

#### `quote add`
**Flags obligatorios**
- `--event-id <id>`
- `--bookmaker-id <id>`
- `--market <moneyline|threeway|spread|total>`
- `--side <HOME|AWAY|DRAW|OVER|UNDER>`
- `--price <decimal>` (cuota decimal, p. ej. `1.95`)
**Flags opcionales**
- `--line <decimal>` (no requerido para `moneyline|threeway`)
- `--asian` (activa modo asiático)
- `--line-b <decimal>` y `--price-b <decimal>` (necesarios si `--asian`)
```bash
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market moneyline \
  --side HOME --price 1.95
```

#### `quote list`
**Flags obligatorios**
- `--event-id <id>`
- `--bookmaker-id <id>`
```bash
./gigamctl quote list --event-id 1 --bookmaker-id 1
```

---

### bet
Registro y listado de apuestas.

#### `bet place`
**Flags obligatorios**
- `--bookmaker-id <id>`
- `--event-id <id>`
- `--runner-id <id>`
- `--bettor-id <id>`
- `--market <moneyline|threeway|spread|total>`
- `--side <HOME|AWAY|DRAW|OVER|UNDER>`
- `--price <decimal>`
- `--stake <cents>`
**Flags opcionales**
- `--line <decimal>` (no requerido para moneyline/threeway)
- `--asian` `--line-b <decimal>` `--price-b <decimal>` (para AHC)
```bash
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 1 \
  --market moneyline --side HOME --price 1.95 --stake 2500
```

#### `bet list`
**Flags obligatorios**
- `--bookmaker-id <id>`
```bash
./gigamctl bet list --bookmaker-id 1
```

---

### settle
Liquidación de apuestas (requiere evento finalizado).

#### `settle event`
**Flags obligatorios**
- `--event-id <id>`
```bash
./gigamctl settle event --event-id 1
```

- Calcula `result`, `payout_cents`, `profit_cents`.
- Registra **comisiones de runner** según esquema (`net` o `handle`) y tasa.

---

### report
Reportes con **formatos** y **exportación**.

**Flags comunes**
- `--bookmaker-id <id>` (obligatorio)
- `--from <YYYY-MM-DD>` (obligatorio)
- `--to <YYYY-MM-DD>` (obligatorio)
- `--format table|json|csv` (opcional; por defecto `table`)
- `--out <file>` (opcional; **solo** para `json`/`csv`; sobrescribe sin preguntar)

> `table` siempre imprime a `stdout` e **ignora** `--out`.

#### `report pnl`
KPIs de apuestas liquidadas en el rango.

**Flags opcionales**
- `--by runner|bettor` (agrupa y ordena por utilidad, runner DESC / bettor ASC)

**Ejemplos**
```bash
# Global (tabla a stdout)
./gigamctl report pnl --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31

# Por runner, a CSV
./gigamctl report pnl --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --by runner --format csv --out reports/pnl_by_runner_oct.csv

# Por bettor, JSON a stdout
./gigamctl report pnl --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --by bettor --format json
```

Columnas típicas:
- Global: `bets`, `handle_usd`, `profit_usd`
- Por *runner*: `runner_id`, `name`, `bets`, `handle_usd`, `profit_usd`
- Por *bettor*: `bettor_id`, `code`, `bets`, `handle_usd`, `profit_usd`

#### `report runner-commissions`
Comisiones generadas por runner en el rango.

**Ejemplos**
```bash
./gigamctl report runner-commissions --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31
./gigamctl report runner-commissions --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --format csv --out reports/runner_commissions_oct.csv
```

Columnas: `runner_id`, `name`, `commissions_usd`, `items`.

#### `report bettor-balances`
Saldo por apostador: lo **adeudado** menos lo **pagado**.

**Ejemplos**
```bash
./gigamctl report bettor-balances --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31
./gigamctl report bettor-balances --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --format json --out reports/bettor_balances_oct.json
```

Columnas: `bettor_id`, `code`, `owed_gross_usd`, `paid_usd`, `balance_usd`.

#### `report runner-balances`
Saldo por runner: **comisiones** menos **pagos**.

**Ejemplos**
```bash
./gigamctl report runner-balances --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31
./gigamctl report runner-balances --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --format csv --out reports/runner_balances_oct.csv
```

Columnas: `runner_id`, `name`, `commissions_usd`, `paid_usd`, `balance_usd`.

**Notas de formato (`report`)**
- `json`: array `[{...}, {...}]` con claves = alias de columnas.
- `csv`: primera fila encabezados; escapado de comillas/comas/nuevas líneas.
- `table`: usa el formato nativo del CLI (stdout).
- `--out` **falla** si el directorio no existe (no crea árboles).

---

### risk
Estimación de exposición por escenarios básicos de resultado.

#### `risk list`
**Flags obligatorios**
- `--event-id <id>`

**Ejemplo**
```bash
./gigamctl risk list --event-id 1
```

Salida (USD):
```
Scenario         Exposure_USD
HOME_wins        -12.50
AWAY_wins         30.00
DRAW               0.00
OVER               0.00
UNDER              0.00
```

> Nota: Es un enfoque simplificado por mercado/escenario.

---

## Códigos de salida

- `0`  Éxito
- `1`  Uso incorrecto del comando raíz (sin subcomando)
- `2`  Error de validación/parsing de flags o subcomando desconocido
- `5`  Error de base de datos u operación dependiente
- `-1` Error genérico durante ejecución de consulta en algunos paths

---

## Ejemplo de sesión “smoke test”

> Crea datos mínimos, apuesta, liquida y saca reportes en distintos formatos.

```bash
# Build
make clean && make

# Semilla
./gigamctl sport create --name Soccer
./gigamctl league create --sport-id 1 --name "CR Primera"
./gigamctl team create --league-id 1 --name "Alajuelense"
./gigamctl team create --league-id 1 --name "Saprissa"
./gigamctl bookmaker create --name "DemoBook" --currency USD
./gigamctl runner create --bookmaker-id 1 --user runner1 --name "Runner 1" --default --scheme net --rate 12
./gigamctl bettor create --runner-id 1 --code B001 --name "Alice"
./gigamctl bettor create --runner-id 1 --code B002 --name "Bob"
./gigamctl event create --league-id 1 --starts-at "2025-10-30 20:00" --home-id 1 --away-id 2

# Cuotas y apuestas
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market moneyline --side HOME --price 1.95
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market moneyline --side AWAY --price 2.05
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 1 \
  --market moneyline --side HOME --price 1.95 --stake 2500
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 2 \
  --market moneyline --side AWAY --price 2.05 --stake 3000

# Finaliza y liquida
./gigamctl event set-score --event-id 1 --home 2 --away 1 --final
./gigamctl settle event --event-id 1

# Reportes
FROM="2025-10-30"; TO="2025-10-30"; mkdir -p reports
./gigamctl report pnl --bookmaker-id 1 --from "$FROM" --to "$TO"
./gigamctl report pnl --bookmaker-id 1 --from "$FROM" --to "$TO" --format json
./gigamctl report pnl --bookmaker-id 1 --from "$FROM" --to "$TO" --format csv \
  --out reports/pnl_global.csv

./gigamctl report pnl --bookmaker-id 1 --from "$FROM" --to "$TO" --by runner --format csv \
  --out reports/pnl_by_runner.csv
./gigamctl report runner-commissions --bookmaker-id 1 --from "$FROM" --to "$TO" --format csv \
  --out reports/runner_commissions.csv
./gigamctl report bettor-balances --bookmaker-id 1 --from "$FROM" --to "$TO" --format csv \
  --out reports/bettor_balances.csv
./gigamctl report runner-balances --bookmaker-id 1 --from "$FROM" --to "$TO" --format json \
  --out reports/runner_balances.json
```
