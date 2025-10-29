#!/usr/bin/env bash
# Hard reset: DROP and CREATE database, then re-run migrations.

set -euo pipefail

CONFIRM=${1:-}
if [[ "$CONFIRM" != "--yes" ]]; then
  echo "Safety check: run with '--yes' to proceed (this DROPS the database!)."
  exit 2
fi

: "${DB_HOST:=127.0.0.1}"
: "${DB_PORT:=3306}"
: "${DB_NAME:=gigam_db}"
: "${DB_USER:=gigam_user}"
: "${DB_PASS:=gigam_pass}"

: "${DB_ROOT_USER:=root}"
: "${DB_ROOT_PASS:=}"

if [[ -z "${DB_ROOT_PASS}" ]]; then
  echo "ERROR: DB_ROOT_PASS is empty. Export DB_ROOT_USER/DB_ROOT_PASS and retry."
  exit 1
fi

echo ">> HARD RESET on ${DB_NAME}@${DB_HOST}:${DB_PORT}"
mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_ROOT_USER" -p"$DB_ROOT_PASS" -e "DROP DATABASE IF EXISTS \\`${DB_NAME}\\`;"
mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_ROOT_USER" -p"$DB_ROOT_PASS" -e "CREATE DATABASE \\`${DB_NAME}\\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"

echo ">> Applying migrations..."
./scripts/migrate.sh

echo "OK: hard reset complete."
