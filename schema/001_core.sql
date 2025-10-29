-- Core schema (clean, MySQL 5.7/8.0 compatible)

CREATE TABLE IF NOT EXISTS sports (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  name VARCHAR(128) NOT NULL UNIQUE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS leagues (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  sport_id BIGINT NOT NULL,
  name VARCHAR(128) NOT NULL,
  UNIQUE KEY uq_league (sport_id, name),
  CONSTRAINT fk_league_sport FOREIGN KEY (sport_id) REFERENCES sports(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS teams (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  league_id BIGINT NOT NULL,
  name VARCHAR(128) NOT NULL,
  UNIQUE KEY uq_team (league_id, name),
  CONSTRAINT fk_team_league FOREIGN KEY (league_id) REFERENCES leagues(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS users (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(128) NOT NULL UNIQUE,
  display_name VARCHAR(128) NOT NULL
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS bookmakers (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  name VARCHAR(128) NOT NULL UNIQUE,
  currency VARCHAR(8) NOT NULL DEFAULT 'USD',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS runners (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  bookmaker_id BIGINT NOT NULL,
  name VARCHAR(128) NOT NULL,
  is_default TINYINT NOT NULL DEFAULT 0,
  commission_scheme ENUM('net','handle') NOT NULL DEFAULT 'net',
  commission_rate DECIMAL(5,2) NOT NULL DEFAULT 10.00,
  UNIQUE KEY uq_runner (user_id, bookmaker_id),
  CONSTRAINT fk_runner_user FOREIGN KEY (user_id) REFERENCES users(id),
  CONSTRAINT fk_runner_book FOREIGN KEY (bookmaker_id) REFERENCES bookmakers(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS bettors (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  runner_id BIGINT NOT NULL,
  code VARCHAR(64) NOT NULL UNIQUE,
  display_name VARCHAR(128) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_bettor_runner FOREIGN KEY (runner_id) REFERENCES runners(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS events (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  league_id BIGINT NOT NULL,
  starts_at DATETIME NOT NULL,
  home_team_id BIGINT NOT NULL,
  away_team_id BIGINT NOT NULL,
  status ENUM('scheduled','live','final') NOT NULL DEFAULT 'scheduled',
  home_score INT NULL,
  away_score INT NULL,
  CONSTRAINT fk_event_league FOREIGN KEY (league_id) REFERENCES leagues(id),
  CONSTRAINT fk_event_home FOREIGN KEY (home_team_id) REFERENCES teams(id),
  CONSTRAINT fk_event_away FOREIGN KEY (away_team_id) REFERENCES teams(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS quotes (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  event_id BIGINT NOT NULL,
  bookmaker_id BIGINT NOT NULL,
  market_type ENUM('moneyline','threeway','spread','total') NOT NULL,
  side ENUM('HOME','AWAY','DRAW','OVER','UNDER') NOT NULL,
  line DOUBLE NULL,
  is_asian TINYINT NOT NULL DEFAULT 0,
  line_b DOUBLE NULL,
  price_decimal DOUBLE NOT NULL,
  price_decimal_b DOUBLE NULL,
  captured_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  KEY idx_quotes_evt (event_id),
  CONSTRAINT fk_quote_event FOREIGN KEY (event_id) REFERENCES events(id),
  CONSTRAINT fk_quote_book FOREIGN KEY (bookmaker_id) REFERENCES bookmakers(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS bets (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  bookmaker_id BIGINT NOT NULL,
  event_id BIGINT NOT NULL,
  quote_id BIGINT NULL,
  placed_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  stake_cents BIGINT NOT NULL,
  market_type ENUM('moneyline','threeway','spread','total') NOT NULL,
  pick_side ENUM('HOME','AWAY','DRAW','OVER','UNDER') NOT NULL,
  line DOUBLE NULL,
  is_asian TINYINT NOT NULL DEFAULT 0,
  price_decimal DOUBLE NOT NULL,
  price_decimal_b DOUBLE NULL,
  line_b DOUBLE NULL,
  runner_id BIGINT NOT NULL,
  bettor_id BIGINT NOT NULL,
  status ENUM('open','settled','void') NOT NULL DEFAULT 'open',
  result ENUM('win','lose','push') NULL,
  payout_cents BIGINT NULL,
  profit_cents BIGINT NULL,
  settled_at DATETIME NULL,
  KEY idx_bets_evt (event_id),
  KEY idx_bets_runner (runner_id),
  KEY idx_bets_bettor (bettor_id),
  CONSTRAINT fk_bet_book FOREIGN KEY (bookmaker_id) REFERENCES bookmakers(id),
  CONSTRAINT fk_bet_event FOREIGN KEY (event_id) REFERENCES events(id),
  CONSTRAINT fk_bet_quote FOREIGN KEY (quote_id) REFERENCES quotes(id),
  CONSTRAINT fk_bet_runner FOREIGN KEY (runner_id) REFERENCES runners(id),
  CONSTRAINT fk_bet_bettor FOREIGN KEY (bettor_id) REFERENCES bettors(id)
) ENGINE=InnoDB;

