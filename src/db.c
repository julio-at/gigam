#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* env_or(const char* k, const char* d) {
  const char* v = getenv(k);
  return v && *v ? v : d;
}

void db_load_env(db_config_t* cfg) {
  snprintf(cfg->host, sizeof(cfg->host), "%s", env_or("DB_HOST","127.0.0.1"));
  cfg->port = atoi(env_or("DB_PORT","3306"));
  snprintf(cfg->dbname, sizeof(cfg->dbname), "%s", env_or("DB_NAME","gigam_db"));
  snprintf(cfg->user, sizeof(cfg->user), "%s", env_or("DB_USER","gigam_user"));
  snprintf(cfg->pass, sizeof(cfg->pass), "%s", env_or("DB_PASS","gigam_pass"));
}

MYSQL* db_connect(const db_config_t* cfg) {
  MYSQL* c = mysql_init(NULL);
  if (!c) return NULL;
  if (!mysql_real_connect(c, cfg->host, cfg->user, cfg->pass, cfg->dbname, cfg->port, NULL, 0)) {
    fprintf(stderr, "MySQL connect error: %s\n", mysql_error(c));
    mysql_close(c);
    return NULL;
  }
  return c;
}

void db_disconnect(MYSQL* conn) {
  if (conn) mysql_close(conn);
}

int db_exec(MYSQL* conn, const char* sql) {
  if (mysql_query(conn, sql) != 0) {
    fprintf(stderr, "SQL error: %s\n", mysql_error(conn));
    return -1;
  }
  return 0;
}

void db_print_result(MYSQL_RES* res) {
  if (!res) return;
  unsigned int n = mysql_num_fields(res);
  MYSQL_FIELD* fields = mysql_fetch_fields(res);
  for (unsigned int i=0;i<n;i++) {
    printf("%s%s", fields[i].name, (i+1<n) ? "\t" : "\n");
  }
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    unsigned long* lengths = mysql_fetch_lengths(res);
    for (unsigned int i=0;i<n;i++) {
      if (row[i]) fwrite(row[i], 1, lengths[i], stdout);
      else printf("NULL");
      printf("%s", (i+1<n) ? "\t" : "\n");
    }
  }
}
