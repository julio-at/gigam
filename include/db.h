#ifndef GIGAM_DB_H
#define GIGAM_DB_H

#include <mysql/mysql.h>
#include <stddef.h>

typedef struct {
  char host[128];
  int  port;
  char dbname[128];
  char user[128];
  char pass[128];
} db_config_t;

void db_load_env(db_config_t* cfg);
MYSQL* db_connect(const db_config_t* cfg);
void db_disconnect(MYSQL* conn);
int db_exec(MYSQL* conn, const char* sql);
void db_print_result(MYSQL_RES* res);

int cli_dispatch(int argc, char** argv);

#endif
