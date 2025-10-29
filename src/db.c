#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_load_env(db_config_t* cfg) {
  if (!cfg) return -1;
  memset(cfg, 0, sizeof(*cfg));

  const char* v;
  strncpy(cfg->host, (v=getenv("DB_HOST"))?v:"127.0.0.1", sizeof(cfg->host)-1);
  cfg->port = (unsigned int)(getenv("DB_PORT") ? atoi(getenv("DB_PORT")) : 3306);
  strncpy(cfg->user, (v=getenv("DB_USER"))?v:"gigam_user", sizeof(cfg->user)-1);
  strncpy(cfg->pass, (v=getenv("DB_PASS"))?v:"gigam_pass", sizeof(cfg->pass)-1);
  strncpy(cfg->name, (v=getenv("DB_NAME"))?v:"gigam_db", sizeof(cfg->name)-1);
  strncpy(cfg->tz,   (v=getenv("DB_TIMEZONE"))?v:"+00:00", sizeof(cfg->tz)-1);

  return 0;
}

MYSQL* db_connect(const db_config_t* cfg) {
  if (!cfg) return NULL;
  MYSQL* c = mysql_init(NULL);
  if (!c) return NULL;

  if (!mysql_real_connect(c, cfg->host, cfg->user, cfg->pass,
                          cfg->name, cfg->port, NULL, 0)) {
    fprintf(stderr, "MySQL connect error: %s\n", mysql_error(c));
    mysql_close(c);
    return NULL;
  }

  /* Set time zone if provided */
  char tzsql[32];
  snprintf(tzsql, sizeof(tzsql), "SET time_zone='%s'", cfg->tz[0]?cfg->tz:"+00:00");
  if (mysql_query(c, tzsql)) {
    fprintf(stderr, "Warning: cannot set time zone: %s\n", mysql_error(c));
  }

  return c;
}

void db_disconnect(MYSQL* conn) {
  if (conn) mysql_close(conn);
}

int db_exec(MYSQL* conn, const char* sql) {
  if (mysql_query(conn, sql)) {
    fprintf(stderr, "SQL error: %s\n", mysql_error(conn));
    return -1;
  }
  return 0;
}

void db_print_result(MYSQL_RES* res) {
  if (!res) return;
  unsigned int n = mysql_num_fields(res);
  MYSQL_FIELD* f = mysql_fetch_fields(res);
  for (unsigned int i=0; i<n; i++) {
    printf("%s%s", f[i].name, (i+1<n) ? "\t" : "\n");
  }
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    for (unsigned int i=0; i<n; i++) {
      printf("%s%s", row[i]?row[i]:"", (i+1<n) ? "\t" : "\n");
    }
  }
}
