#!/usr/bin/env bash
set -euo pipefail

: "${DB_HOST:=127.0.0.1}"
: "${DB_PORT:=3306}"
: "${DB_NAME:=gigam_db}"
: "${DB_USER:=gigam_user}"
: "${DB_PASS:=gigam_pass}"

mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASS" "$DB_NAME" < schema/001_core.sql
echo "OK schema migrated."
