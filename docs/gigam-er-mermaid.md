# Gigam Core Schema — ER (Mermaid)

```mermaid
erDiagram
  SPORTS {
    BIGINT id PK
    VARCHAR name "UNIQUE"
  }

  LEAGUES {
    BIGINT id PK
    BIGINT sport_id FK
    VARCHAR name
    -- UNIQUE (sport_id, name)
  }

  TEAMS {
    BIGINT id PK
    BIGINT league_id FK
    VARCHAR name
    -- UNIQUE (league_id, name)
  }

  USERS {
    BIGINT id PK
    VARCHAR username "UNIQUE"
    VARCHAR display_name
  }

  BOOKMAKERS {
    BIGINT id PK
    VARCHAR name "UNIQUE"
    VARCHAR currency "DEFAULT 'USD'"
    TIMESTAMP created_at "DEFAULT CURRENT_TIMESTAMP"
  }

  RUNNERS {
    BIGINT id PK
    BIGINT user_id FK
    BIGINT bookmaker_id FK
    VARCHAR name
    TINYINT is_default "DEFAULT 0"
    ENUM commission_scheme "('net','handle') DEFAULT 'net'"
    DECIMAL commission_rate "DECIMAL(5,2) DEFAULT 10.00"
    -- UNIQUE (user_id, bookmaker_id)
  }

  BETTORS {
    BIGINT id PK
    BIGINT runner_id FK
    VARCHAR code "UNIQUE"
    VARCHAR display_name
    TIMESTAMP created_at "DEFAULT CURRENT_TIMESTAMP"
  }

  EVENTS {
    BIGINT id PK
    BIGINT league_id FK
    DATETIME starts_at
    BIGINT home_team_id FK
    BIGINT away_team_id FK
    ENUM status "('scheduled','live','final') DEFAULT 'scheduled'"
    INT home_score
    INT away_score
  }

  QUOTES {
    BIGINT id PK
    BIGINT event_id FK
    BIGINT bookmaker_id FK
    ENUM market_type "('moneyline','threeway','spread','total')"
    ENUM side "('HOME','AWAY','DRAW','OVER','UNDER')"
    DOUBLE line
    TINYINT is_asian "DEFAULT 0"
    DOUBLE line_b
    DOUBLE price_decimal
    DOUBLE price_decimal_b
    TIMESTAMP captured_at "DEFAULT CURRENT_TIMESTAMP"
    -- KEY (event_id)
  }

  BETS {
    BIGINT id PK
    BIGINT bookmaker_id FK
    BIGINT event_id FK
    BIGINT quote_id FK
    TIMESTAMP placed_at "DEFAULT CURRENT_TIMESTAMP"
    BIGINT stake_cents
    ENUM market_type "('moneyline','threeway','spread','total')"
    ENUM pick_side "('HOME','AWAY','DRAW','OVER','UNDER')"
    DOUBLE line
    TINYINT is_asian "DEFAULT 0"
    DOUBLE price_decimal
    DOUBLE price_decimal_b
    DOUBLE line_b
    BIGINT runner_id FK
    BIGINT bettor_id FK
    ENUM status "('open','settled','void') DEFAULT 'open'"
    ENUM result "('win','lose','push')"
    BIGINT payout_cents
    BIGINT profit_cents
    DATETIME settled_at
    -- KEY (event_id), KEY (runner_id), KEY (bettor_id)
  }

  %% Relationships
  SPORTS ||--o{ LEAGUES : has
  LEAGUES ||--o{ TEAMS : has
  LEAGUES ||--o{ EVENTS : schedules
  TEAMS ||--o{ EVENTS : home_team
  TEAMS ||--o{ EVENTS : away_team

  USERS ||--o{ RUNNERS : owns
  BOOKMAKERS ||--o{ RUNNERS : employs

  RUNNERS ||--o{ BETTORS : manages

  EVENTS ||--o{ QUOTES : offered_for
  BOOKMAKERS ||--o{ QUOTES : posts

  BOOKMAKERS ||--o{ BETS : takes
  EVENTS ||--o{ BETS : on
  QUOTES ||--o{ BETS : references
  RUNNERS ||--o{ BETS : places_for
  BETTORS ||--o{ BETS : placed_by
```
