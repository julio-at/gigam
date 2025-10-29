#!/usr/bin/env bash
# Soft reset: truncate all data but keep the schema.

set -euo pipefail

: "${DB_HOST:=127.0.0.1}"
: "${DB_PORT:=3306}"
: "${DB_NAME:=gigam_db}"
: "${DB_USER:=gigam_user}"
: "${DB_PASS:=gigam_pass}"

SQL=$(cat <<'SQL_EOF'
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
SQL_EOF
)

echo ">> Soft reset (truncate) on ${DB_NAME}@${DB_HOST}:${DB_PORT}"
mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "$SQL"
echo "OK: all tables truncated."
