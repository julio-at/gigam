#!/usr/bin/env bash
# Seed minimal demo data for smoke tests (idempotent).

set -euo pipefail

: "${DB_HOST:=127.0.0.1}"
: "${DB_PORT:=3306}"
: "${DB_NAME:=gigam_db}"
: "${DB_USER:=gigam_user}"
: "${DB_PASS:=gigam_pass}"

run_sql () {
  mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "$1"
}

echo ">> Seeding demo data..."

run_sql "INSERT INTO sports(name) VALUES('Soccer') ON DUPLICATE KEY UPDATE name=VALUES(name);"
run_sql "INSERT INTO leagues(sport_id,name) SELECT id,'Premier Demo' FROM sports WHERE name='Soccer' ON DUPLICATE KEY UPDATE name=VALUES(name);"
run_sql "INSERT INTO teams(league_id,name) SELECT l.id,'Team Alpha' FROM leagues l WHERE l.name='Premier Demo' ON DUPLICATE KEY UPDATE name=VALUES(name);"
run_sql "INSERT INTO teams(league_id,name) SELECT l.id,'Team Beta'  FROM leagues l WHERE l.name='Premier Demo' ON DUPLICATE KEY UPDATE name=VALUES(name);"

run_sql "INSERT INTO bookmakers(name,currency) VALUES('DemoBook','USD') ON DUPLICATE KEY UPDATE currency=VALUES(currency);"
run_sql "INSERT INTO users(username,display_name) VALUES('runner1','Default Runner') ON DUPLICATE KEY UPDATE display_name=VALUES(display_name);"
run_sql "INSERT INTO runners(user_id,bookmaker_id,name,is_default) SELECT u.id,b.id,'Default Runner',1 FROM users u, bookmakers b WHERE u.username='runner1' AND b.name='DemoBook' ON DUPLICATE KEY UPDATE name=VALUES(name), is_default=VALUES(is_default);"

run_sql "INSERT INTO bettors(runner_id,code,display_name) SELECT r.id,'B1001','John Bettor' FROM runners r WHERE r.is_default=1 LIMIT 1 ON DUPLICATE KEY UPDATE display_name=VALUES(display_name);"

echo "OK: seed complete."
