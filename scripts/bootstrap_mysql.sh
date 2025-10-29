#!/usr/bin/env bash
set -euo pipefail

: "${DB_HOST:=127.0.0.1}"
: "${DB_PORT:=3306}"
: "${DB_ROOT_USER:=root}"
: "${DB_ROOT_PASS:=}"
: "${DB_NAME:=gigam_db}"
: "${DB_USER:=gigam_user}"
: "${DB_PASS:=gigam_pass}"

mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_ROOT_USER" ${DB_ROOT_PASS:+-p"$DB_ROOT_PASS"} -e "
CREATE DATABASE IF NOT EXISTS \`$DB_NAME\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS '$DB_USER'@'%' IDENTIFIED BY '$DB_PASS';
GRANT ALL PRIVILEGES ON \`$DB_NAME\`.* TO '$DB_USER'@'%';
FLUSH PRIVILEGES;
"

echo "OK bootstrap done for $DB_NAME ($DB_USER)."
