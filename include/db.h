#ifndef GIGAM_DB_H
#define GIGAM_DB_H

#include <mysql/mysql.h>

typedef struct {
  char host[128];
  unsigned int port;
  char user[64];
  char pass[128];
  char name[128];
  char tz[8];   /* e.g., +00:00 */
} db_config_t;

int  db_load_env(db_config_t* cfg);
MYSQL* db_connect(const db_config_t* cfg);
void db_disconnect(MYSQL* conn);

/* Small helpers */
int db_exec(MYSQL* conn, const char* sql);
void db_print_result(MYSQL_RES* res);

#endif
