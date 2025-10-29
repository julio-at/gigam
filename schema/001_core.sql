-- Minimal PoC schema (no triggers, no views for MVP)
CREATE TABLE IF NOT EXISTS bookmakers (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(128) NOT NULL UNIQUE,
  currency CHAR(3) NOT NULL DEFAULT 'USD',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS users (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(64) NOT NULL UNIQUE,
  display_name VARCHAR(128) NOT NULL DEFAULT '',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS runners (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id BIGINT NOT NULL,
  bookmaker_id BIGINT NOT NULL,
  name VARCHAR(128) NOT NULL,
  is_default TINYINT(1) NOT NULL DEFAULT 0,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uk_runner_user_bm (user_id, bookmaker_id),
  CONSTRAINT fk_runner_user FOREIGN KEY(user_id) REFERENCES users(id),
  CONSTRAINT fk_runner_bm FOREIGN KEY(bookmaker_id) REFERENCES bookmakers(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS bettors (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  runner_id BIGINT NOT NULL,
  code VARCHAR(32) NOT NULL,
  display_name VARCHAR(128) NOT NULL DEFAULT '',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uk_bettor_runner_code (runner_id, code),
  CONSTRAINT fk_bettor_runner FOREIGN KEY(runner_id) REFERENCES runners(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS sports (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(64) NOT NULL UNIQUE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS leagues (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  sport_id BIGINT NOT NULL,
  name VARCHAR(128) NOT NULL,
  UNIQUE KEY uk_league_sport_name (sport_id, name),
  CONSTRAINT fk_league_sport FOREIGN KEY(sport_id) REFERENCES sports(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS teams (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  league_id BIGINT NOT NULL,
  name VARCHAR(128) NOT NULL,
  UNIQUE KEY uk_team_league_name (league_id, name),
  CONSTRAINT fk_team_league FOREIGN KEY(league_id) REFERENCES leagues(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS events (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  league_id BIGINT NOT NULL,
  starts_at DATETIME NOT NULL,
  home_team_id BIGINT NOT NULL,
  away_team_id BIGINT NOT NULL,
  status ENUM('scheduled','live','final','canceled') NOT NULL DEFAULT 'scheduled',
  home_score INT NULL,
  away_score INT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_event_league FOREIGN KEY(league_id) REFERENCES leagues(id),
  CONSTRAINT fk_event_home FOREIGN KEY(home_team_id) REFERENCES teams(id),
  CONSTRAINT fk_event_away FOREIGN KEY(away_team_id) REFERENCES teams(id)
) ENGINE=InnoDB;

-- quotes/lines per bookmaker
CREATE TABLE IF NOT EXISTS quotes (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  event_id BIGINT NOT NULL,
  bookmaker_id BIGINT NOT NULL,
  market_type ENUM('moneyline','threeway','spread','total') NOT NULL,
  side ENUM('HOME','AWAY','DRAW','OVER','UNDER') NOT NULL,
  line DECIMAL(6,2) NULL,
  is_asian TINYINT(1) NOT NULL DEFAULT 0,
  line_b DECIMAL(6,2) NULL,
  price_decimal DECIMAL(8,4) NOT NULL,
  price_decimal_b DECIMAL(8,4) NULL,
  notes VARCHAR(255) NULL,
  captured_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_quotes_event_bm (event_id, bookmaker_id),
  CONSTRAINT fk_quote_event FOREIGN KEY(event_id) REFERENCES events(id),
  CONSTRAINT fk_quote_bm FOREIGN KEY(bookmaker_id) REFERENCES bookmakers(id)
) ENGINE=InnoDB;

-- simple bets (stake in cents for integer safety)
CREATE TABLE IF NOT EXISTS bets (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  bookmaker_id BIGINT NOT NULL,
  event_id BIGINT NOT NULL,
  quote_id BIGINT NULL,
  stake_cents BIGINT NOT NULL,
  market_type ENUM('moneyline','threeway','spread','total') NOT NULL,
  pick_side ENUM('HOME','AWAY','DRAW','OVER','UNDER') NOT NULL,
  line DECIMAL(6,2) NULL,
  is_asian TINYINT(1) NOT NULL DEFAULT 0,
  price_decimal DECIMAL(8,4) NOT NULL,
  price_decimal_b DECIMAL(8,4) NULL,
  runner_id BIGINT NOT NULL,
  bettor_id BIGINT NOT NULL,
  status ENUM('open','settled','void') NOT NULL DEFAULT 'open',
  placed_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_bet_bm FOREIGN KEY(bookmaker_id) REFERENCES bookmakers(id),
  CONSTRAINT fk_bet_event FOREIGN KEY(event_id) REFERENCES events(id),
  CONSTRAINT fk_bet_quote FOREIGN KEY(quote_id) REFERENCES quotes(id),
  CONSTRAINT fk_bet_runner FOREIGN KEY(runner_id) REFERENCES runners(id),
  CONSTRAINT fk_bet_bettor FOREIGN KEY(bettor_id) REFERENCES bettors(id)
) ENGINE=InnoDB;
