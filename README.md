# GIGAM (PoC/MVP)

Minimal C CLI (no TUI) to manage a sports betting PoC on MySQL.

## Build

```bash
cp .env.example .env
# export env (or source your own)
export $(grep -v '^#' .env | xargs)

make clean && make
```

## Bootstrap & Schema

```bash
# If you need to create DB/user (requires root access):
DB_ROOT_USER=root DB_ROOT_PASS="" ./scripts/bootstrap_mysql.sh

# Apply schema:
./scripts/migrate.sh
```

## CLI Usage (examples)

```bash
# Bookmaker
./gigamctl bookmaker create --name "DemoBook" --currency USD
./gigamctl bookmaker list

# Runner
./gigamctl runner create --bookmaker-id 1 --user runner1 --name "Default Runner" --default
./gigamctl runner list --bookmaker-id 1
./gigamctl runner set-default --bookmaker-id 1 --runner-id 1

# Bettor
./gigamctl bettor create --runner-id 1 --code B1001 --name "John Bettor"
./gigamctl bettor list --runner-id 1

# Event (you should have sports/leagues/teams already in DB)
./gigamctl event create --league-id 1 --starts-at "2025-11-02 18:00:00" --home-id 1 --away-id 2
./gigamctl event list

# Quote
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market spread --side HOME --line -1.5 --price 1.90
./gigamctl quote list --event-id 1 --bookmaker-id 1

# Bet
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 1 --market spread --side HOME --line -1.5 --price 1.90 --stake 2500
./gigamctl bet list --bookmaker-id 1
```

> This is a PoC: no triggers/views/accounting/settlement yet. Add later as separate steps.
