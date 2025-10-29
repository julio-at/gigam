# gigam (PoC / MVP)

Minimal console CLI in C (MySQL) to manage sports betting entities and operations.

## Features
- Entities: sports, leagues, teams, bookmakers, runners, bettors, events, quotes, bets
- Quotes: spread, total, moneyline, threeway, asian split (.25/.75)
- Bets: place/list
- Event: set-score/finalize
- Settle: settle event bets into win/lose/push with payout/profit
- Runner commissions: per-bet (net or handle)
- Payout ledger: runner & bettor payouts
- Reports: P&L, runner commissions, bettor/runner balances
- Risk: exposure scenarios per event (negative = book loss, positive = book gain)

## Build
```bash
sudo apt-get update
sudo apt-get install -y build-essential pkg-config libmysqlclient-dev
cp .env.example .env
export $(grep -v '^#' .env | xargs)

./scripts/migrate.sh
make clean && make
```

## CLI Quickstart
```bash
# Sports/League/Teams
./gigamctl sport create --name "Soccer"
./gigamctl sport list
./gigamctl league create --sport-id 1 --name "Premier Demo"
./gigamctl league list --sport-id 1
./gigamctl team create --league-id 1 --name "Team Alpha"
./gigamctl team create --league-id 1 --name "Team Beta"
./gigamctl team list --league-id 1

# Bookmaker/Runner/Bettor
./gigamctl bookmaker create --name "DemoBook" --currency USD
./gigamctl runner create --bookmaker-id 1 --user runner1 --name "Default Runner" --default
./gigamctl bettor create --runner-id 1 --code B1001 --name "John Bettor"

# Event + Quotes + Bets
./gigamctl event create --league-id 1 --starts-at "2025-11-02 18:00:00" --home-id 1 --away-id 2
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market spread --side HOME --line -0.75 --price 1.92 --asian --line-b -0.50 --price-b 1.86
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market total --side OVER --line 2.5 --price 1.85
./gigamctl quote add --event-id 1 --bookmaker-id 1 --market moneyline --side HOME --price 1.75
./gigamctl bet place --bookmaker-id 1 --event-id 1 --runner-id 1 --bettor-id 1 --market spread --side HOME --line -0.75 --price 1.92 --stake 2500 --asian --line-b -0.50 --price-b 1.86
./gigamctl bet list --bookmaker-id 1

# Risk, Finalize and Settle
./gigamctl risk list --event-id 1
./gigamctl event set-score --event-id 1 --home 2 --away 1 --final
./gigamctl settle event --event-id 1

# Reports and payouts
./gigamctl report pnl --bookmaker-id 1 --from 2025-11-01 --to 2025-11-30
./gigamctl report pnl --bookmaker-id 1 --from 2025-11-01 --to 2025-11-30 --by runner
./gigamctl report runner-commissions --bookmaker-id 1 --from 2025-11-01 --to 2025-11-30
./gigamctl report bettor-balances --bookmaker-id 1 --from 2025-11-01 --to 2025-11-30
./gigamctl report runner-balances --bookmaker-id 1 --from 2025-11-01 --to 2025-11-30

./gigamctl runner payout --runner-id 1 --amount-cents 1500 --note "Weekly payout" --from 2025-11-01 --to 2025-11-07
./gigamctl bettor payout --bettor-id 1 --amount-cents 2500 --note "Win 11/02" --from 2025-11-01 --to 2025-11-07
```

## Reset DB (truncate all)
```sql
SET FOREIGN_KEY_CHECKS=0;
TRUNCATE TABLE runner_commissions;
TRUNCATE TABLE payouts_runner;
TRUNCATE TABLE payouts_bettor;
TRUNCATE TABLE bets;
TRUNCATE TABLE quotes;
TRUNCATE TABLE events;
TRUNCATE TABLE teams;
TRUNCATE TABLE leagues;
TRUNCATE TABLE sports;
TRUNCATE TABLE bettors;
TRUNCATE TABLE runners;
TRUNCATE TABLE users;
TRUNCATE TABLE bookmakers;
SET FOREIGN_KEY_CHECKS=1;
```

## Reset & Seed
```bash
# Soft reset (truncate all tables)
./scripts/reset_truncate.sh

# Hard reset (drop/recreate DB + migrations)
DB_ROOT_USER=root DB_ROOT_PASS='********' ./scripts/reset_hard.sh --yes

# Seed minimal demo
./scripts/seed_demo.sh
```
