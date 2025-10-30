# gigamctl — User Manual

CLI to manage entities and core operations of a betting system (PoC).  
Includes create/list for entities, quote & bet ingestion, event settlement, formatted reports, and risk overview.

> **This manual version** matches the integration that adds `--format {table|json|csv}` and `--out <file>` to `report`.

---

## Table of Contents

1. [Requirements & Setup](#requirements--setup)  
2. [General Conventions](#general-conventions)  
3. [Commands & Subcommands](#commands--subcommands)  
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
4. [Exit Codes](#exit-codes)  
5. [“Smoke Test” Example Session](#smoke-test-example-session)

---

## Requirements & Setup

- Reachable **MySQL/MariaDB** database.  
- Environment variables read by `gigamctl` via `db_load_env()` (adjust to your env):

```bash
export DB_HOST=127.0.0.1
export DB_PORT=3306
export DB_USER=root
export DB_PASS=yourpassword
export DB_NAME=gigam
```

- Build in the project root:

```bash
make clean && make
```

The resulting binary is `./gigamctl`.

---

## General Conventions

- **Money amounts** are handled internally in **cents** (e.g., `2500` = `25.00 USD`) unless the SQL already returns a USD field.
- **Date** format: `YYYY-MM-DD`. When time is included: `YYYY-MM-DD HH:MM[:SS]`.
- For `--from` / `--to` ranges: the filter is `from <= date < (to + 1 day)`.  
  Example: `--from 2025-10-01 --to 2025-10-31` covers the whole October 2025.
- Unless specified otherwise, **list** commands return key columns ordered by `id` or report relevance.

---

## Commands & Subcommands

### sport
Create and list sports.

#### `sport create`
**Required**
- `--name <string>`: sport name.  
**Example**
```bash
./gigamctl sport create --name Soccer
```

#### `sport list`
List all sports.
```bash
./gigamctl sport list
```

---

### league
Create and list leagues.

#### `league create`
**Required**
- `--sport-id <id>`: sport id.
- `--name <string>`: league name.
```bash
./gigamctl league create --sport-id 1 --name "CR Primera"
```

#### `league list`
**Optional**
- `--sport-id <id>`: filter by sport id.
```bash
./gigamctl league list
./gigamctl league list --sport-id 1
```

---

### team
Create and list teams.

#### `team create`
**Required**
- `--league-id <id>`
- `--name <string>`
```bash
./gigamctl team create --league-id 1 --name "Alajuelense"
```

#### `team list`
**Optional**
- `--league-id <id>`
```bash
./gigamctl team list
./gigamctl team list --league-id 1
```

---

### bookmaker
Create and list bookmakers.

#### `bookmaker create`
**Flags**
- `--name <string>` (required)
- `--currency <ISO>` (optional, default `USD`)
```bash
./gigamctl bookmaker create --name "DemoBook" --currency USD
```

#### `bookmaker list`
```bash
./gigamctl bookmaker list
```

---

### runner
Manage runners (operators).

#### `runner create`
**Required**
- `--bookmaker-id <id>`
- `--user <username>`
- `--name <display_name>`
**Optional**
- `--default` (mark as default)
- `--scheme net|handle` (default `net`)
- `--rate <0..100>` (default `10.0`)
```bash
./gigamctl runner create --bookmaker-id 1 --user runner1 \
  --name "Runner 1" --default --scheme net --rate 12
```

#### `runner list`
**Required**
- `--bookmaker-id <id>`
```bash
./gigamctl runner list --bookmaker-id 1
```

#### `runner set-default`
**Required**
- `--bookmaker-id <id>`
- `--runner-id <id>`
```bash
./gigamctl runner set-default --bookmaker-id 1 --runner-id 1
```

#### `runner payout`
Register payouts to a runner (settled commissions).

**Required**
- `--runner-id <id>`
- `--amount-cents <int>`
**Optional**
- `--note <string>`
- `--from <YYYY-MM-DD>` `--to <YYYY-MM-DD>` (associated period)
```bash
./gigamctl runner payout --runner-id 1 --amount-cents 500 --note "partial" \
  --from 2025-10-01 --to 2025-10-31
```

---

### bettor
Manage bettors.

#### `bettor create`
**Required**
- `--runner-id <id>`
- `--code <string>`
**Optional**
- `--name <display_name>`
```bash
./gigamctl bettor create --runner-id 1 --code B001 --name "Alice"
```

#### `bettor list`
**Required**
- `--runner-id <id>`
```bash
./gigamctl bettor list --runner-id 1
```

#### `bettor payout`
Register payouts to a bettor.

**Required**
- `--bettor-id <id>`
- `--amount-cents <int>`
**Optional**
- `--note <string>`
- `--from <YYYY-MM-DD>` `--to <YYYY-MM-DD>`
```bash
./gigamctl bettor payout --bettor-id 1 --amount-cents 1000 --note "settlement" \
  --from 2025-10-01 --to 2025-10-31
```

---

### event
Manage events.

#### `event create`
**Required**
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
Update score; `--final` marks the event as final.

**Required**
- `--event-id <id>`
**Optional**
- `--home <int>` `--away <int>`
- `--final`
```bash
./gigamctl event set-score --event-id 1 --home 2 --away 1 --final
```

#### `event finalize`
Mark an event as final.

**Required**
- `--event-id <id>`
```bash
./gigamctl event finalize --event-id 1
```

---

### quote
Manage price quotes.

#### `quote add`
**Required**
- `--event-id <id>`
- `--bookmaker-id <id>`
- `--market <moneyline|threeway|spread|total>`
- `--side <HOME|AWAY|DRAW|OVER|UNDER>`
- `--price <decimal>` (e.g., `1.95`)
**Optional**
- `--line <decimal>` (not required for `moneyline|threeway`)
- `--asian`
- `--line-b <decimal>` and `--price-b <decimal>` (required when `--asian`)
```bash
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market moneyline \
  --side HOME --price 1.95
```

#### `quote list`
**Required**
- `--event-id <id>`
- `--bookmaker-id <id>`
```bash
./gigamctl quote list --event-id 1 --bookmaker-id 1
```

---

### bet
Register and list bets.

#### `bet place`
**Required**
- `--bookmaker-id <id>`
- `--event-id <id>`
- `--runner-id <id>`
- `--bettor-id <id>`
- `--market <moneyline|threeway|spread|total>`
- `--side <HOME|AWAY|DRAW|OVER|UNDER>`
- `--price <decimal>`
- `--stake <cents>`
**Optional**
- `--line <decimal>` (not required for moneyline/threeway)
- `--asian` `--line-b <decimal>` `--price-b <decimal>` (Asian handicap split)
```bash
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 1 \
  --market moneyline --side HOME --price 1.95 --stake 2500
```

#### `bet list`
**Required**
- `--bookmaker-id <id>`
```bash
./gigamctl bet list --bookmaker-id 1
```

---

### settle
Settle bets (event must be final).

#### `settle event`
**Required**
- `--event-id <id>`
```bash
./gigamctl settle event --event-id 1
```

- Computes `result`, `payout_cents`, `profit_cents`.
- Inserts **runner commissions** according to scheme (`net` or `handle`) and rate.

---

### report
Reports with **formats** and **export**.

**Common flags**
- `--bookmaker-id <id>` (required)
- `--from <YYYY-MM-DD>` (required)
- `--to <YYYY-MM-DD>` (required)
- `--format table|json|csv` (optional; default `table`)
- `--out <file>` (optional; **only** for `json`/`csv`; overwrites if exists)

> `table` always prints to `stdout` and **ignores** `--out`.

#### `report pnl`
KPIs for settled bets in the range.

**Optional**
- `--by runner|bettor` (group & sort: runner DESC profit, bettor ASC profit)

**Examples**
```bash
# Global (table to stdout)
./gigamctl report pnl --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31

# By runner, CSV
./gigamctl report pnl --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --by runner --format csv --out reports/pnl_by_runner_oct.csv

# By bettor, JSON to stdout
./gigamctl report pnl --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --by bettor --format json
```

Typical columns:
- Global: `bets`, `handle_usd`, `profit_usd`
- By *runner*: `runner_id`, `name`, `bets`, `handle_usd`, `profit_usd`
- By *bettor*: `bettor_id`, `code`, `bets`, `handle_usd`, `profit_usd`

#### `report runner-commissions`
Runner commissions in the range.

**Examples**
```bash
./gigamctl report runner-commissions --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31
./gigamctl report runner-commissions --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --format csv --out reports/runner_commissions_oct.csv
```

Columns: `runner_id`, `name`, `commissions_usd`, `items`.

#### `report bettor-balances`
Balance per bettor: **owed** minus **paid**.

**Examples**
```bash
./gigamctl report bettor-balances --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31
./gigamctl report bettor-balances --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --format json --out reports/bettor_balances_oct.json
```

Columns: `bettor_id`, `code`, `owed_gross_usd`, `paid_usd`, `balance_usd`.

#### `report runner-balances`
Balance per runner: **commissions** minus **payouts**.

**Examples**
```bash
./gigamctl report runner-balances --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31
./gigamctl report runner-balances --bookmaker-id 1 --from 2025-10-01 --to 2025-10-31 \
  --format csv --out reports/runner_balances_oct.csv
```

Columns: `runner_id`, `name`, `commissions_usd`, `paid_usd`, `balance_usd`.

**Formatting notes (`report`)**
- `json`: array `[{...}, {...}]` with keys = column aliases.
- `csv`: header line + rows; proper escaping of quotes/commas/newlines.
- `table`: native CLI table to `stdout`.
- `--out` **fails** if directory does not exist (no directory creation).

---

### risk
Exposure estimation for basic outcome scenarios.

#### `risk list`
**Required**
- `--event-id <id>`

**Example**
```bash
./gigamctl risk list --event-id 1
```

Output (USD):
```
Scenario         Exposure_USD
HOME_wins        -12.50
AWAY_wins         30.00
DRAW               0.00
OVER               0.00
UNDER              0.00
```

> Note: simplified per-market/per-scenario view.

---

## Exit Codes

- `0`  Success
- `1`  Root command misuse (no subcommand)
- `2`  Flag/validation error or unknown subcommand
- `5`  Database or dependent operation error
- `-1` Generic error during some execution paths

---

## “Smoke Test” Example Session

> Create minimal data, place bets, settle, and extract reports in all formats.

```bash
# Build
make clean && make

# Seed
./gigamctl sport create --name Soccer
./gigamctl league create --sport-id 1 --name "CR Primera"
./gigamctl team create --league-id 1 --name "Alajuelense"
./gigamctl team create --league-id 1 --name "Saprissa"
./gigamctl bookmaker create --name "DemoBook" --currency USD
./gigamctl runner create --bookmaker-id 1 --user runner1 --name "Runner 1" --default --scheme net --rate 12
./gigamctl bettor create --runner-id 1 --code B001 --name "Alice"
./gigamctl bettor create --runner-id 1 --code B002 --name "Bob"
./gigamctl event create --league-id 1 --starts-at "2025-10-30 20:00" --home-id 1 --away-id 2

# Quotes & bets
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market moneyline --side HOME --price 1.95
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market moneyline --side AWAY --price 2.05
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 1 \
  --market moneyline --side HOME --price 1.95 --stake 2500
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 2 \
  --market moneyline --side AWAY --price 2.05 --stake 3000

# Finalize & settle
./gigamctl event set-score --event-id 1 --home 2 --away 1 --final
./gigamctl settle event --event-id 1

# Reports
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
