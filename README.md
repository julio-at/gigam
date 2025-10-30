# gigamctl

Command-line tool (PoC) to manage entities and core operations of a betting system: create/list entities, ingest quotes & bets, settle events, generate reports (with export), and review basic risk exposure.

> This README references the English and Spanish manuals for full details. The CLI matches the integration that adds `--format {table|json|csv}` and `--out <file>` to `report`.

---

## Requirements

- **MySQL/MariaDB** reachable.
- Build essentials (GCC/Clang, Make).

### Environment variables

`gigamctl` reads DB settings via `db_load_env()`:

```bash
export DB_HOST=127.0.0.1
export DB_PORT=3306
export DB_USER=root
export DB_PASS=yourpassword
export DB_NAME=gigam
```

---

## Build

```bash
make clean && make
```
The binary will be available as `./gigamctl`.

---

## Quick Start

Create minimal data, place bets, finalize an event, settle, and run reports.

```bash
# 1) Seed minimal data
./gigamctl sport create --name Soccer
./gigamctl league create --sport-id 1 --name "CR Primera"
./gigamctl team create --league-id 1 --name "Alajuelense"
./gigamctl team create --league-id 1 --name "Saprissa"
./gigamctl bookmaker create --name "DemoBook" --currency USD
./gigamctl runner create --bookmaker-id 1 --user runner1 --name "Runner 1" --default --scheme net --rate 12
./gigamctl bettor create --runner-id 1 --code B001 --name "Alice"
./gigamctl bettor create --runner-id 1 --code B002 --name "Bob"

# 2) Event + quotes + bets
./gigamctl event create --league-id 1 --starts-at "2025-10-30 20:00" --home-id 1 --away-id 2
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market moneyline --side HOME --price 1.95
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market moneyline --side AWAY --price 2.05
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 1 --market moneyline --side HOME --price 1.95 --stake 2500
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 2 --market moneyline --side AWAY --price 2.05 --stake 3000

# 3) Finalize + settle
./gigamctl event set-score --event-id 1 --home 2 --away 1 --final
./gigamctl settle event --event-id 1

# 4) Reports (table/json/csv; json/csv support --out)
FROM="2025-10-30"; TO="2025-10-30"; mkdir -p reports
./gigamctl report pnl --bookmaker-id 1 --from "$FROM" --to "$TO"
./gigamctl report pnl --bookmaker-id 1 --from "$FROM" --to "$TO" --format json
./gigamctl report pnl --bookmaker-id 1 --from "$FROM" --to "$TO" --format csv --out reports/pnl_global.csv

./gigamctl report pnl --bookmaker-id 1 --from "$FROM" --to "$TO" --by runner --format csv --out reports/pnl_by_runner.csv
./gigamctl report runner-commissions --bookmaker-id 1 --from "$FROM" --to "$TO" --format csv --out reports/runner_commissions.csv
./gigamctl report bettor-balances --bookmaker-id 1 --from "$FROM" --to "$TO" --format csv --out reports/bettor_balances.csv
./gigamctl report runner-balances --bookmaker-id 1 --from "$FROM" --to "$TO" --format json --out reports/runner_balances.json
```

---

## Reporting & Export

All `report` subcommands share common flags:
- `--bookmaker-id <id>` (required)
- `--from <YYYY-MM-DD>` (required)
- `--to <YYYY-MM-DD>` (required)
- `--format table|json|csv` (optional; default `table`)
- `--out <file>` (optional; only for `json`/`csv`; overwrites if exists)

> The date filter is `from <= date < (to + 1 day)`; e.g., `--from 2025-10-01 --to 2025-10-31` covers the whole October 2025.  
> `table` always prints to `stdout` and ignores `--out`.

---

## Documentation

- **English Manual:** [MANUAL.en.md](./docs/MANUAL.en.md)  
- **Spanish Manual:** [MANUAL.es.md](./docs/MANUAL.es.md)

---

## Notes

- Money amounts are stored internally in **cents** unless the query already returns USD fields.
- When using `--out`, ensure the target directory exists (no auto-creation).
- Settlement inserts runner commissions according to runner scheme/rate.
