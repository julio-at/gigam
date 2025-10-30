#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

/* ---------- Helpers / UX ---------- */

static void usage_root(void) {
  fprintf(stderr,
    "gigamctl (PoC)\n"
    "Commands:\n"
    "  sport     create|list\n"
    "  league    create|list\n"
    "  team      create|list\n"
    "  bookmaker create|list\n"
    "  runner    create|list|set-default|payout\n"
    "            create flags: --bookmaker-id --user --name [--default] [--scheme net|handle] [--rate <0..100>]\n"
    "  bettor    create|list|payout\n"
    "  event     create|list|set-score|finalize\n"
    "  quote     add|list\n"
    "  bet       place|list\n"
    "  settle    event\n"
    "  report    pnl|runner-commissions|bettor-balances|runner-balances [--format table|json|csv] [--out <file>]\n"
    "  risk      list\n"
  );
}

static int exec_and_print(MYSQL* c, const char* sql) {
  if (db_exec(c, sql) != 0) {
    return -1;
  }
  MYSQL_RES* r = mysql_store_result(c);
  if (r) {
    db_print_result(r);
    mysql_free_result(r);
  }
  return 0;
}

static void esc_str(MYSQL* c, const char* in, char* out, size_t outsz) {
  if (!in) { out[0]='\0'; return; }
  size_t n = strlen(in);
  char* tmp = (char*)malloc(n*2+1);
  if(!tmp){ out[0]='\0'; return; }
  unsigned long m = mysql_real_escape_string(c, tmp, in, n);
  if ((size_t)m >= outsz) m = (unsigned long)(outsz - 1);
  memcpy(out, tmp, (size_t)m);
  out[m]='\0';
  free(tmp);
}

/* ---------- Report formatting helpers (table|json|csv) ---------- */

typedef enum {
  RF_TABLE = 0,
  RF_JSON  = 1,
  RF_CSV   = 2
} rf_format_t;

static rf_format_t rf_format_from_str(const char* s) {
  if (!s) return RF_TABLE;
  char buf[16]; size_t n=0;
  for (; s[n] && n<sizeof(buf)-1; ++n) {
    char ch = s[n];
    if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
    buf[n] = ch;
  }
  buf[n]=0;
  if (strcmp(buf,"json")==0) return RF_JSON;
  if (strcmp(buf,"csv")==0)  return RF_CSV;
  return RF_TABLE;
}

static void json_escape(FILE* f, const char* s) {
  if (!s) s="";
  fputc('"', f);
  for (const unsigned char* p=(const unsigned char*)s; *p; ++p) {
    unsigned char ch=*p;
    switch(ch){
      case '\\': fputs("\\\\",f); break;
      case '\"': fputs("\\\"",f); break;
      case '\b': fputs("\\b", f); break;
      case '\f': fputs("\\f", f); break;
      case '\n': fputs("\\n", f); break;
      case '\r': fputs("\\r", f); break;
      case '\t': fputs("\\t", f); break;
      default:
        if (ch < 0x20) fprintf(f,"\\u%04x", ch);
        else fputc(ch,f);
    }
  }
  fputc('"', f);
}

static void csv_escape(FILE* f, const char* s) {
  if (!s) s="";
  bool need_quotes=false;
  for (const char* p=s; *p; ++p) {
    if (*p==',' || *p=='"' || *p=='\n' || *p=='\r') { need_quotes=true; break; }
  }
  if (!need_quotes && s[0] != ' ') { fputs(s, f); return; }
  fputc('"', f);
  for (const char* p=s; *p; ++p) {
    if (*p=='"') fputc('"', f);
    fputc(*p, f);
  }
  fputc('"', f);
}

static int print_result_json(MYSQL_RES* r, FILE* out) {
  unsigned int nf = mysql_num_fields(r);
  MYSQL_FIELD* flds = mysql_fetch_fields(r);
  fputs("[\n", out);
  bool first=true;
  MYSQL_ROW row;
  while ((row=mysql_fetch_row(r))) {
    unsigned long* lengths = mysql_fetch_lengths(r);
    (void)lengths; /* values are C-strings; lengths unused for now */
    if (!first) fputs(",\n", out);
    first=false;
    fputs("  {", out);
    for (unsigned int i=0;i<nf;i++) {
      if (i) fputs(", ", out);
      json_escape(out, flds[i].name ? flds[i].name : "");
      fputs(": ", out);
      json_escape(out, row[i] ? row[i] : "");
    }
    fputs("}", out);
  }
  fputs("\n]\n", out);
  return 0;
}

static int print_result_csv(MYSQL_RES* r, FILE* out) {
  unsigned int nf = mysql_num_fields(r);
  MYSQL_FIELD* flds = mysql_fetch_fields(r);
  /* header */
  for (unsigned int i=0;i<nf;i++) {
    if (i) fputc(',', out);
    csv_escape(out, flds[i].name ? flds[i].name : "");
  }
  fputc('\n', out);
  /* rows */
  MYSQL_ROW row;
  while ((row=mysql_fetch_row(r))) {
    for (unsigned int i=0;i<nf;i++) {
      if (i) fputc(',', out);
      csv_escape(out, row[i] ? row[i] : "");
    }
    fputc('\n', out);
  }
  return 0;
}

static int print_result_table(MYSQL_RES* r) {
  /* mantener comportamiento existente */
  db_print_result(r);
  return 0;
}

static int print_formatted(MYSQL_RES* r, rf_format_t fmt, const char* out_path) {
  if (!r) return 0;
  if (fmt == RF_TABLE) {
    /* table siempre va a stdout, ignoramos out_path */
    return print_result_table(r);
  }
  FILE* out = stdout;
  FILE* opened = NULL;
  if (out_path && *out_path) {
    opened = fopen(out_path, "wb");
    if (!opened) {
      fprintf(stderr, "report: unable to open output file: %s\n", out_path);
      return 1;
    }
    out = opened;
  }
  int rc = 0;
  if (fmt == RF_JSON) rc = print_result_json(r, out);
  else                rc = print_result_csv(r, out);

  if (opened) fclose(opened);
  return rc;
}

/* ---------- SPORT ---------- */

static int cmd_sport(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr, "sport create|list\n"); return 2; }
  const char* sub = argv[1]; optind=1;

  if (!strcmp(sub,"create")) {
    const char* name=NULL; static struct option o[]={{"name",1,0,'n'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"n:",o,&ix))!=-1){
      if(ch=='n') name=optarg; else return 2;
    }
    if(!name){
      fprintf(stderr,"required: --name\n");
      return 2;
    }
    char ne[256]; esc_str(c,name,ne,sizeof(ne));
    char q[512];
    snprintf(q,sizeof(q),"INSERT INTO sports(name) VALUES('%s') ON DUPLICATE KEY UPDATE name=VALUES(name)", ne);
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"list")) {
    return exec_and_print(c,"SELECT id,name FROM sports ORDER BY id");
  }

  fprintf(stderr,"unknown sport subcommand\n");
  return 2;
}

/* ---------- LEAGUE ---------- */

static int cmd_league(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr, "league create|list\n"); return 2; }
  const char* sub = argv[1]; optind=1;

  if (!strcmp(sub,"create")) {
    long sport_id=0; const char* name=NULL;
    static struct option o[]={{"sport-id",1,0,'s'},{"name",1,0,'n'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"s:n:",o,&ix))!=-1){
      if(ch=='s') sport_id=atol(optarg);
      else if(ch=='n') name=optarg;
      else return 2;
    }
    if(!sport_id||!name){
      fprintf(stderr,"required: --sport-id --name\n");
      return 2;
    }
    char ne[256]; esc_str(c,name,ne,sizeof(ne));
    char q[1024];
    snprintf(q,sizeof(q),"INSERT INTO leagues(sport_id,name) VALUES(%ld,'%s') ON DUPLICATE KEY UPDATE name=VALUES(name)", sport_id, ne);
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"list")) {
    long sport_id=0; static struct option o[]={{"sport-id",1,0,'s'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"s:",o,&ix))!=-1){
      if(ch=='s') sport_id=atol(optarg); else return 2;
    }
    if (sport_id>0) {
      char q[512];
      snprintf(q,sizeof(q),"SELECT id,name,sport_id FROM leagues WHERE sport_id=%ld ORDER BY id", sport_id);
      return exec_and_print(c,q);
    } else {
      return exec_and_print(c,"SELECT id,name,sport_id FROM leagues ORDER BY id");
    }
  }

  fprintf(stderr,"unknown league subcommand\n");
  return 2;
}

/* ---------- TEAM ---------- */

static int cmd_team(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr, "team create|list\n"); return 2; }
  const char* sub=argv[1]; optind=1;

  if (!strcmp(sub,"create")) {
    long league_id=0; const char* name=NULL;
    static struct option o[]={{"league-id",1,0,'l'},{"name",1,0,'n'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"l:n:",o,&ix))!=-1){
      if(ch=='l') league_id=atol(optarg);
      else if(ch=='n') name=optarg;
      else return 2;
    }
    if(!league_id||!name){
      fprintf(stderr,"required: --league-id --name\n");
      return 2;
    }
    char ne[256]; esc_str(c,name,ne,sizeof(ne));
    char q[1024];
    snprintf(q,sizeof(q),"INSERT INTO teams(league_id,name) VALUES(%ld,'%s') ON DUPLICATE KEY UPDATE name=VALUES(name)", league_id, ne);
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"list")) {
    long league_id=0; static struct option o[]={{"league-id",1,0,'l'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"l:",o,&ix))!=-1){
      if(ch=='l') league_id=atol(optarg); else return 2;
    }
    if (league_id>0) {
      char q[512];
      snprintf(q,sizeof(q),"SELECT id,name,league_id FROM teams WHERE league_id=%ld ORDER BY id", league_id);
      return exec_and_print(c,q);
    } else {
      return exec_and_print(c,"SELECT id,name,league_id FROM teams ORDER BY id");
    }
  }

  fprintf(stderr,"unknown team subcommand\n");
  return 2;
}

/* ---------- BOOKMAKER ---------- */

static int cmd_bookmaker(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr, "bookmaker create|list\n"); return 2; }
  const char* sub=argv[1]; optind=1;

  if (!strcmp(sub,"create")) {
    const char* name=NULL; const char* currency="USD";
    static struct option o[]={{"name",1,0,'n'},{"currency",1,0,'c'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"n:c:",o,&ix))!=-1){
      if(ch=='n') name=optarg;
      else if(ch=='c') currency=optarg;
      else return 2;
    }
    if(!name){
      fprintf(stderr,"required: --name\n");
      return 2;
    }
    char ne[256], ce[16]; esc_str(c,name,ne,sizeof(ne)); esc_str(c,currency,ce,sizeof(ce));
    char q[1024];
    snprintf(q,sizeof(q),"INSERT INTO bookmakers(name,currency) VALUES('%s','%s') ON DUPLICATE KEY UPDATE currency=VALUES(currency)", ne, ce);
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"list")) {
    return exec_and_print(c,"SELECT id,name,currency,created_at FROM bookmakers ORDER BY id");
  }

  fprintf(stderr,"unknown bookmaker subcommand\n");
  return 2;
}

/* ---------- RUNNER ---------- */

static int cmd_runner(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr, "runner create|list|set-default|payout\n"); return 2; }
  const char* sub=argv[1]; optind=1;

  if (!strcmp(sub,"create")) {
    long bm=0; const char* user=NULL; const char* name=NULL; int is_def=0;
    const char* scheme="net"; /* default matches DB */
    double rate=10.0;         /* default matches DB */

    static struct option o[]={
      {"bookmaker-id",1,0,'b'},
      {"user",1,0,'u'},
      {"name",1,0,'n'},
      {"default",0,0,'d'},
      {"scheme",1,0,'s'},   /* net | handle */
      {"rate",1,0,'r'},     /* percent 0..100 */
      {0,0,0,0}
    };
    int ch,ix=0; optind=1;
    while((ch=getopt_long(argc-1,argv+1,"b:u:n:ds:r:",o,&ix))!=-1){
      if(ch=='b') bm=atol(optarg);
      else if(ch=='u') user=optarg;
      else if(ch=='n') name=optarg;
      else if(ch=='d') is_def=1;
      else if(ch=='s') scheme=optarg;
      else if(ch=='r') rate=atof(optarg);
      else return 2;
    }
    if(!bm||!user||!name){
      fprintf(stderr,"required: --bookmaker-id --user --name [--default] [--scheme net|handle] [--rate 0..100]\n");
      return 2;
    }
    if (strcmp(scheme,"net")!=0 && strcmp(scheme,"handle")!=0) {
      fprintf(stderr,"invalid --scheme (use net|handle)\n");
      return 2;
    }
    if (rate < 0.0 || rate > 100.0) {
      fprintf(stderr,"invalid --rate (0..100)\n");
      return 2;
    }

    char ue[256], ne[256]; esc_str(c,user,ue,sizeof(ue)); esc_str(c,name,ne,sizeof(ne));
    char q1[1024];
    snprintf(q1,sizeof(q1),
      "INSERT INTO users(username,display_name) VALUES('%s','%s') "
      "ON DUPLICATE KEY UPDATE display_name=VALUES(display_name)", ue, ne);
    if(db_exec(c,q1)!=0) { return 5; }

    char q2[1024];
    snprintf(q2,sizeof(q2),
      "INSERT INTO runners(user_id,bookmaker_id,name,is_default,commission_scheme,commission_rate) "
      "SELECT id,%ld,'%s',%d,'%s',%.2f FROM users WHERE username='%s' "
      "ON DUPLICATE KEY UPDATE name=VALUES(name), is_default=VALUES(is_default), commission_scheme=VALUES(commission_scheme), commission_rate=VALUES(commission_rate)",
      bm, ne, is_def, scheme, rate, ue);
    if(db_exec(c,q2)!=0) { return 5; }

    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"list")) {
    long bm=0; static struct option o[]={{"bookmaker-id",1,0,'b'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"b:",o,&ix))!=-1){
      if(ch=='b') bm=atol(optarg); else return 2;
    }
    if(!bm){
      fprintf(stderr,"required: --bookmaker-id\n");
      return 2;
    }
    char q[1024];
    snprintf(q,sizeof(q),"SELECT r.id,r.name,r.is_default,u.username, r.commission_scheme, r.commission_rate FROM runners r JOIN users u ON u.id=r.user_id WHERE r.bookmaker_id=%ld ORDER BY r.id", bm);
    return exec_and_print(c,q);
  }

  if (!strcmp(sub,"set-default")) {
    long bm=0, rid=0; static struct option o[]={{"bookmaker-id",1,0,'b'},{"runner-id",1,0,'r'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"b:r:",o,&ix))!=-1){
      if(ch=='b') bm=atol(optarg);
      else if(ch=='r') rid=atol(optarg);
      else return 2;
    }
    if(!bm||!rid){
      fprintf(stderr,"required: --bookmaker-id --runner-id\n");
      return 2;
    }
    char q1[256];
    snprintf(q1,sizeof(q1),"UPDATE runners SET is_default=0 WHERE bookmaker_id=%ld", bm);
    if(db_exec(c,q1)!=0) { return 5; }

    char q2[256];
    snprintf(q2,sizeof(q2),"UPDATE runners SET is_default=1 WHERE id=%ld AND bookmaker_id=%ld", rid, bm);
    if(db_exec(c,q2)!=0) { return 5; }

    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"payout")) {
    long runner=0, amount=0; const char* note=""; const char* from=NULL; const char* to=NULL;
    static struct option o[]={{"runner-id",1,0,'r'},{"amount-cents",1,0,'a'},{"note",1,0,'n'},{"from",1,0,'f'},{"to",1,0,'t'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"r:a:n:f:t:",o,&ix))!=-1){
      if(ch=='r') runner=atol(optarg);
      else if(ch=='a') amount=atol(optarg);
      else if(ch=='n') note=optarg;
      else if(ch=='f') from=optarg;
      else if(ch=='t') to=optarg;
      else return 2;
    }
    if(!runner||!amount){
      fprintf(stderr,"required: --runner-id --amount-cents [--note] [--from --to]\n");
      return 2;
    }
    char ne[256]; esc_str(c,note,ne,sizeof(ne));
    char q[1024];
    if(from&&to){
      char fe[32],te[32]; esc_str(c,from,fe,sizeof(fe)); esc_str(c,to,te,sizeof(te));
      snprintf(q,sizeof(q),
        "INSERT INTO payouts_runner(runner_id,amount_cents,note,period_from,period_to) "
        "VALUES(%ld,%ld,'%s',STR_TO_DATE('%s','%%Y-%%m-%%d'),STR_TO_DATE('%s','%%Y-%%m-%%d'))",
        runner, amount, ne, fe, te);
    } else {
      snprintf(q,sizeof(q),"INSERT INTO payouts_runner(runner_id,amount_cents,note) VALUES(%ld,%ld,'%s')", runner, amount, ne);
    }
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK runner payout recorded\n");
    return 0;
  }

  fprintf(stderr,"unknown runner subcommand\n");
  return 2;
}

/* ---------- BETTOR ---------- */

static int cmd_bettor(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr, "bettor create|list|payout\n"); return 2; }
  const char* sub=argv[1]; optind=1;

  if (!strcmp(sub,"create")) {
    long runner=0; const char* code=NULL; const char* name="";
    static struct option o[]={{"runner-id",1,0,'r'},{"code",1,0,'c'},{"name",1,0,'n'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"r:c:n:",o,&ix))!=-1){
      if(ch=='r') runner=atol(optarg);
      else if(ch=='c') code=optarg;
      else if(ch=='n') name=optarg;
      else return 2;
    }
    if(!runner||!code){
      fprintf(stderr,"required: --runner-id --code [--name]\n");
      return 2;
    }
    char ce[128], ne[256]; esc_str(c,code,ce,sizeof(ce)); esc_str(c,name,ne,sizeof(ne));
    char q[1024];
    snprintf(q,sizeof(q),"INSERT INTO bettors(runner_id,code,display_name) VALUES(%ld,'%s','%s') ON DUPLICATE KEY UPDATE display_name=VALUES(display_name)", runner, ce, ne);
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"list")) {
    long runner=0; static struct option o[]={{"runner-id",1,0,'r'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"r:",o,&ix))!=-1){
      if(ch=='r') runner=atol(optarg); else return 2;
    }
    if(!runner){
      fprintf(stderr,"required: --runner-id\n");
      return 2;
    }
    char q[512];
    snprintf(q,sizeof(q),"SELECT id,code,display_name,created_at FROM bettors WHERE runner_id=%ld ORDER BY id", runner);
    return exec_and_print(c,q);
  }

  if (!strcmp(sub,"payout")) {
    long bettor=0, amount=0; const char* note=""; const char* from=NULL; const char* to=NULL;
    static struct option o[]={{"bettor-id",1,0,'b'},{"amount-cents",1,0,'a'},{"note",1,0,'n'},{"from",1,0,'f'},{"to",1,0,'t'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"b:a:n:f:t:",o,&ix))!=-1){
      if(ch=='b') bettor=atol(optarg);
      else if(ch=='a') amount=atol(optarg);
      else if(ch=='n') note=optarg;
      else if(ch=='f') from=optarg;
      else if(ch=='t') to=optarg;
      else return 2;
    }
    if(!bettor||!amount){
      fprintf(stderr,"required: --bettor-id --amount-cents [--note] [--from --to]\n");
      return 2;
    }
    char ne[256]; esc_str(c,note,ne,sizeof(ne));
    char q[1024];
    if(from&&to){
      char fe[32],te[32]; esc_str(c,from,fe,sizeof(fe)); esc_str(c,to,te,sizeof(te));
      snprintf(q,sizeof(q),
        "INSERT INTO payouts_bettor(bettor_id,amount_cents,note,period_from,period_to) "
        "VALUES(%ld,%ld,'%s',STR_TO_DATE('%s','%%Y-%%m-%%d'),STR_TO_DATE('%s','%%Y-%%m-%%d'))",
        bettor, amount, ne, fe, te);
    } else {
      snprintf(q,sizeof(q),"INSERT INTO payouts_bettor(bettor_id,amount_cents,note) VALUES(%ld,%ld,'%s')", bettor, amount, ne);
    }
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK bettor payout recorded\n");
    return 0;
  }

  fprintf(stderr,"unknown bettor subcommand\n");
  return 2;
}

/* ---------- EVENT ---------- */

static int cmd_event(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr,"event create|list|set-score|finalize\n"); return 2; }
  const char* sub=argv[1]; optind=1;

  if (!strcmp(sub,"create")) {
    long league=0, home=0, away=0; const char* starts=NULL;
    static struct option o[]={{"league-id",1,0,'l'},{"starts-at",1,0,'t'},{"home-id",1,0,'h'},{"away-id",1,0,'a'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"l:t:h:a:",o,&ix))!=-1){
      if(ch=='l') league=atol(optarg);
      else if(ch=='t') starts=optarg;
      else if(ch=='h') home=atol(optarg);
      else if(ch=='a') away=atol(optarg);
      else return 2;
    }
    if(!league||!home||!away||!starts){
      fprintf(stderr,"required: --league-id --starts-at --home-id --away-id\n");
      return 2;
    }
    char se[64]; esc_str(c,starts,se,sizeof(se));
    char q[512];
    snprintf(q,sizeof(q),"INSERT INTO events(league_id,starts_at,home_team_id,away_team_id) VALUES(%ld,'%s',%ld,%ld)", league, se, home, away);
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"list")) {
    return exec_and_print(c,"SELECT id,league_id,DATE_FORMAT(starts_at,'%%Y-%%m-%%d %%H:%%i') AS starts_at,home_team_id,away_team_id,status FROM events ORDER BY starts_at");
  }

  if (!strcmp(sub,"set-score")) {
    long event=0; int home=0, away=0; int fin=0;
    static struct option o[]={{"event-id",1,0,'e'},{"home",1,0,'h'},{"away",1,0,'a'},{"final",0,0,'f'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"e:h:a:f",o,&ix))!=-1){
      if(ch=='e') event=atol(optarg);
      else if(ch=='h') home=atoi(optarg);
      else if(ch=='a') away=atoi(optarg);
      else if(ch=='f') fin=1;
      else return 2;
    }
    if(!event){
      fprintf(stderr,"required: --event-id\n");
      return 2;
    }
    char q[256];
    snprintf(q,sizeof(q),"UPDATE events SET home_score=%d, away_score=%d%s WHERE id=%ld", home, away, fin?", status='final'":"", event);
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"finalize")) {
    long event=0; static struct option o[]={{"event-id",1,0,'e'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"e:",o,&ix))!=-1){
      if(ch=='e') event=atol(optarg); else return 2;
    }
    if(!event){
      fprintf(stderr,"required: --event-id\n");
      return 2;
    }
    char q[256];
    snprintf(q,sizeof(q),"UPDATE events SET status='final' WHERE id=%ld", event);
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK event finalized\n");
    return 0;
  }

  fprintf(stderr,"unknown event subcommand\n");
  return 2;
}

/* ---------- QUOTE ---------- */

static int cmd_quote(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr,"quote add|list\n"); return 2; }
  const char* sub=argv[1]; optind=1;

  if (!strcmp(sub,"add")) {
    long event=0,bm=0; const char* market=NULL; const char* side=NULL;
    double line=0.0,line_b=0.0,price=0.0,price_b=0.0; int asian=0;
    static struct option o[]={
      {"event-id",1,0,'e'},{"bookmaker-id",1,0,'b'},{"market",1,0,'m'},{"side",1,0,'s'},
      {"line",1,0,'l'},{"price",1,0,'p'},{"asian",0,0,'a'},{"line-b",1,0,'L'},{"price-b",1,0,'P'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"e:b:m:s:l:p:aL:P:",o,&ix))!=-1){
      if(ch=='e') event=atol(optarg);
      else if(ch=='b') bm=atol(optarg);
      else if(ch=='m') market=optarg;
      else if(ch=='s') side=optarg;
      else if(ch=='l') line=atof(optarg);
      else if(ch=='p') price=atof(optarg);
      else if(ch=='a') asian=1;
      else if(ch=='L') line_b=atof(optarg);
      else if(ch=='P') price_b=atof(optarg);
      else return 2;
    }
    if(!event||!bm||!market||!side||price<=1.0){
      fprintf(stderr,"required: --event-id --bookmaker-id --market --side --price\n");
      return 2;
    }
    if (asian && (price_b<=1.0)) {
      fprintf(stderr,"asian requires: --price-b (and usually --line-b)\n");
      return 2;
    }
    char me[32], se[16]; esc_str(c,market,me,sizeof(me)); esc_str(c,side,se,sizeof(se));
    char line_sql[32], lineb_sql[32], priceb_sql[32];
    if (!strcmp(me,"moneyline")||!strcmp(me,"threeway")) snprintf(line_sql,sizeof(line_sql),"NULL");
    else snprintf(line_sql,sizeof(line_sql),"%.2f", line);
    if (asian) { snprintf(lineb_sql,sizeof(lineb_sql),"%.2f", line_b); snprintf(priceb_sql,sizeof(priceb_sql),"%.4f", price_b); }
    else { snprintf(lineb_sql,sizeof(lineb_sql),"NULL"); snprintf(priceb_sql,sizeof(priceb_sql),"NULL"); }

    char q[1024];
    snprintf(q,sizeof(q),
      "INSERT INTO quotes(event_id,bookmaker_id,market_type,side,line,is_asian,line_b,price_decimal,price_decimal_b) "
      "VALUES(%ld,%ld,'%s','%s',%s,%d,%s,%.4f,%s)",
      event,bm,me,se,line_sql,asian,lineb_sql,price,priceb_sql);
    if (db_exec(c,q)!=0) { return 5; }
    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"list")) {
    long event=0,bm=0; static struct option o[]={{"event-id",1,0,'e'},{"bookmaker-id",1,0,'b'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"e:b:",o,&ix))!=-1){
      if(ch=='e') event=atol(optarg);
      else if(ch=='b') bm=atol(optarg);
      else return 2;
    }
    if(!event||!bm){
      fprintf(stderr,"required: --event-id --bookmaker-id\n");
      return 2;
    }
    char q[1024];
    snprintf(q,sizeof(q),
      "SELECT id,market_type,side,COALESCE(CAST(line AS CHAR),'-') AS line,is_asian,COALESCE(CAST(line_b AS CHAR),'-') AS line_b,price_decimal,COALESCE(CAST(price_decimal_b AS CHAR),'-') AS price_b,DATE_FORMAT(captured_at,'%%Y-%%m-%%d %%H:%%i:%%s') AS captured_at "
      "FROM quotes WHERE event_id=%ld AND bookmaker_id=%ld ORDER BY id DESC", event, bm);
    return exec_and_print(c,q);
  }

  fprintf(stderr,"unknown quote subcommand\n");
  return 2;
}

/* ---------- BET ---------- */

static int cmd_bet(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr,"bet place|list\n"); return 2; }
  const char* sub=argv[1]; optind=1;

  if (!strcmp(sub,"place")) {
    long bm=0,event=0,runner=0,bettor=0; const char* market=NULL; const char* side=NULL;
    double line=0.0, line_b=0.0, price=0.0, price_b=0.0; long stake=0; int asian=0;
    static struct option o[]={
      {"bookmaker-id",1,0,'b'},{"event-id",1,0,'e'},{"runner-id",1,0,'r'},{"bettor-id",1,0,'t'},
      {"market",1,0,'m'},{"side",1,0,'s'},{"line",1,0,'l'},{"price",1,0,'p'},{"stake",1,0,'k'},
      {"asian",0,0,'a'},{"line-b",1,0,'L'},{"price-b",1,0,'P'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"b:e:r:t:m:s:l:p:k:aL:P:",o,&ix))!=-1){
      if(ch=='b') bm=atol(optarg);
      else if(ch=='e') event=atol(optarg);
      else if(ch=='r') runner=atol(optarg);
      else if(ch=='t') bettor=atol(optarg);
      else if(ch=='m') market=optarg;
      else if(ch=='s') side=optarg;
      else if(ch=='l') line=atof(optarg);
      else if(ch=='p') price=atof(optarg);
      else if(ch=='k') stake=atol(optarg);
      else if(ch=='a') asian=1;
      else if(ch=='L') line_b=atof(optarg);
      else if(ch=='P') price_b=atof(optarg);
      else return 2;
    }
    if(!bm||!event||!runner||!bettor||!market||!side||price<=1.0||stake<=0){
      fprintf(stderr,"required: --bookmaker-id --event-id --runner-id --bettor-id --market --side --price --stake\n");
      return 2;
    }

    char me[32], se[16]; esc_str(c,market,me,sizeof(me)); esc_str(c,side,se,sizeof(se));
    char line_sql[32], priceb_sql[32], lineb_sql[32];
    if(!strcmp(me,"moneyline")||!strcmp(me,"threeway")) snprintf(line_sql,sizeof(line_sql),"NULL");
    else snprintf(line_sql,sizeof(line_sql),"%.2f", line);
    if(asian){ snprintf(priceb_sql,sizeof(priceb_sql),"%.4f", price_b); snprintf(lineb_sql,sizeof(lineb_sql),"%.2f", line_b); }
    else { snprintf(priceb_sql,sizeof(priceb_sql),"NULL"); snprintf(lineb_sql,sizeof(lineb_sql),"NULL"); }

    char qset[1024];
    if(!strcmp(me,"moneyline")||!strcmp(me,"threeway")){
      snprintf(qset,sizeof(qset),
        "SET @qid := (SELECT id FROM quotes WHERE event_id=%ld AND bookmaker_id=%ld AND market_type='%s' AND side='%s' ORDER BY id DESC LIMIT 1);",
        event,bm,me,se);
    } else {
      snprintf(qset,sizeof(qset),
        "SET @qid := (SELECT id FROM quotes WHERE event_id=%ld AND bookmaker_id=%ld AND market_type='%s' AND side='%s' AND COALESCE(line,0)=%.2f ORDER BY id DESC LIMIT 1);",
        event,bm,me,se,line);
    }
    if (db_exec(c,qset)!=0) { return 5; }

    char ins[1024];
    snprintf(ins,sizeof(ins),
      "INSERT INTO bets(bookmaker_id,event_id,quote_id,stake_cents,market_type,pick_side,line,is_asian,price_decimal,price_decimal_b,line_b,runner_id,bettor_id,status) "
      "VALUES(%ld,%ld,@qid,%ld,'%s','%s',%s,%d,%.4f,%s,%s,%ld,%ld,'open')",
      bm,event,stake,me,se,line_sql,asian,price,priceb_sql,lineb_sql,runner,bettor);
    if (db_exec(c,ins)!=0) { return 5; }
    printf("OK\n");
    return 0;
  }

  if (!strcmp(sub,"list")) {
    long bm=0; static struct option o[]={{"bookmaker-id",1,0,'b'},{0,0,0,0}};
    int ch,ix=0;
    while((ch=getopt_long(argc-1,argv+1,"b:",o,&ix))!=-1){
      if(ch=='b') bm=atol(optarg); else return 2;
    }
    if(!bm){
      fprintf(stderr,"required: --bookmaker-id\n");
      return 2;
    }
    char q[1024];
    snprintf(q,sizeof(q),
      "SELECT b.id AS bet_id, DATE_FORMAT(b.placed_at,'%%Y-%%m-%%d %%H:%%i:%%s') AS placed_at, b.market_type,b.pick_side,COALESCE(CAST(b.line AS CHAR),'-') AS line,b.is_asian,b.price_decimal,ROUND(b.stake_cents/100,2) AS stake_usd,b.status "
      "FROM bets b WHERE b.bookmaker_id=%ld ORDER BY b.id DESC LIMIT 200", bm);
    return exec_and_print(c,q);
  }

  fprintf(stderr,"unknown bet subcommand\n");
  return 2;
}

/* ---------- SETTLEMENT HELPERS ---------- */

static int get_event_scores(MYSQL* c, long event_id, int* home, int* away, int* is_final) {
  char q[256]; snprintf(q,sizeof(q),"SELECT home_score,away_score,(status='final') FROM events WHERE id=%ld", event_id);
  if (db_exec(c,q)!=0) return -1;
  MYSQL_RES* r = mysql_store_result(c); if(!r) return -1;
  MYSQL_ROW row = mysql_fetch_row(r);
  if (!row) { mysql_free_result(r); return -1; }
  *home = row[0]?atoi(row[0]):0;
  *away = row[1]?atoi(row[1]):0;
  *is_final = row[2]?atoi(row[2]):0;
  mysql_free_result(r);
  return 0;
}

static void settle_one_leg(double stake_cents_half, double price, int cmp, long long* out_payout, long long* out_profit) {
  long long stake = (long long)(stake_cents_half + 0.5);
  if (cmp > 0) {
    long long payout = (long long)(stake * price + 0.5);
    long long profit = payout - stake;
    *out_payout += payout;
    *out_profit += profit;
  } else if (cmp == 0) {
    *out_payout += stake; /* push */
  } else {
    *out_profit -= stake; /* lose -> book wins stake (negative for player) */
  }
}

/* ---------- SETTLE ---------- */

static int settle_compute(
  const char* market, const char* side, int is_asian,
  double line, double line_b, double price, double price_b,
  long long stake_cents, int home_score, int away_score,
  long long* payout_cents, long long* profit_cents, const char** out_result)
{
  *payout_cents=0; *profit_cents=0; *out_result="lose";
  int cmp=0, cmp_b=0;

  if (!strcmp(market,"moneyline")) {
    int w = (home_score>away_score)?1:(home_score<away_score?-1:0);
    if (!strcmp(side,"HOME")) cmp=(w>0)?1:(w==0?0:-1);
    else if (!strcmp(side,"AWAY")) cmp=(w<0)?1:(w==0?0:-1);
    else if (!strcmp(side,"DRAW")) cmp=(w==0)?1:-1;
    if (w==0 && (strcmp(side,"DRAW")!=0)) cmp=0;
    settle_one_leg((double)stake_cents, price, cmp, payout_cents, profit_cents);
  } else if (!strcmp(market,"threeway")) {
    int w = (home_score>away_score)?1:(home_score<away_score?-1:0);
    if (!strcmp(side,"HOME")) cmp=(w>0)?1:-1;
    else if (!strcmp(side,"AWAY")) cmp=(w<0)?1:-1;
    else if (!strcmp(side,"DRAW")) cmp=(w==0)?1:-1;
    settle_one_leg((double)stake_cents, price, cmp, payout_cents, profit_cents);
  } else if (!strcmp(market,"spread")) {
    if (!strcmp(side,"HOME")) {
      if (is_asian && (line - (int)line != 0.0)) {
        double adj1 = (double)home_score + line;
        double adj2 = (double)home_score + line_b;
        cmp   = (adj1>away_score)?1:((adj1==away_score)?0:-1);
        cmp_b = (adj2>away_score)?1:((adj2==away_score)?0:-1);
        settle_one_leg(stake_cents*0.5, price, cmp, payout_cents, profit_cents);
        settle_one_leg(stake_cents*0.5, price_b>1.0?price_b:price, cmp_b, payout_cents, profit_cents);
      } else {
        double adj = (double)home_score + line;
        cmp = (adj>away_score)?1:((adj==away_score)?0:-1);
        settle_one_leg((double)stake_cents, price, cmp, payout_cents, profit_cents);
      }
    } else {
      if (is_asian && (line - (int)line != 0.0)) {
        double adj1 = (double)away_score + line;
        double adj2 = (double)away_score + line_b;
        cmp   = (adj1>home_score)?1:((adj1==home_score)?0:-1);
        cmp_b = (adj2>home_score)?1:((adj2==home_score)?0:-1);
        settle_one_leg(stake_cents*0.5, price, cmp, payout_cents, profit_cents);
        settle_one_leg(stake_cents*0.5, price_b>1.0?price_b:price, cmp_b, payout_cents, profit_cents);
      } else {
        double adj = (double)away_score + line;
        cmp = (adj>home_score)?1:((adj==home_score)?0:-1);
        settle_one_leg((double)stake_cents, price, cmp, payout_cents, profit_cents);
      }
    }
  } else if (!strcmp(market,"total")) {
    int sum = home_score + away_score;
    if (!strcmp(side,"OVER")) cmp = (sum>line)?1:((sum==line)?0:-1);
    else cmp = (sum<line)?1:((sum==line)?0:-1);
    if (is_asian && (line - (int)line != 0.0)) {
      int cmp1 = (!strcmp(side,"OVER"))? ((sum>line)?1:((sum==line)?0:-1)) : ((sum<line)?1:((sum==line)?0:-1));
      int cmp2 = (!strcmp(side,"OVER"))? ((sum>line_b)?1:((sum==line_b)?0:-1)) : ((sum<line_b)?1:((sum==line_b)?0:-1));
      settle_one_leg(stake_cents*0.5, price, cmp1, payout_cents, profit_cents);
      settle_one_leg(stake_cents*0.5, price_b>1.0?price_b:price, cmp2, payout_cents, profit_cents);
    } else {
      settle_one_leg((double)stake_cents, price, cmp, payout_cents, profit_cents);
    }
  }

  if (*profit_cents > 0) *out_result="win";
  else if (*profit_cents < 0) *out_result="lose";
  else *out_result="push";
  return 0;
}

static int cmd_settle(int argc, char** argv, MYSQL* c) {
  if (argc<2 || strcmp(argv[1],"event")!=0){
    fprintf(stderr,"settle event --event-id X\n"); return 2;
  }
  long event=0; static struct option o[]={{"event-id",1,0,'e'},{0,0,0,0}}; int ch,ix=0; optind=1;
  while((ch=getopt_long(argc-1,argv+1,"e:",o,&ix))!=-1){
    if(ch=='e') event=atol(optarg); else return 2;
  }
  if(!event){
    fprintf(stderr,"required: --event-id\n");
    return 2;
  }
  int hs=0,as=0,fin=0;
  if(get_event_scores(c,event,&hs,&as,&fin)!=0){
    fprintf(stderr,"event not found\n");
    return 5;
  }
  if(!fin){
    fprintf(stderr,"event not final; use event set-score --final\n");
    return 2;
  }

  char qb[256];
  snprintf(qb,sizeof(qb),
    "SELECT id,market_type,pick_side,COALESCE(line,0),is_asian,COALESCE(line_b,0),price_decimal,COALESCE(price_decimal_b,0),stake_cents,runner_id "
    "FROM bets WHERE event_id=%ld AND status='open'", event);
  if (db_exec(c,qb)!=0) { return 5; }
  MYSQL_RES* r = mysql_store_result(c); if(!r){ printf("No open bets.\n"); return 0; }

  MYSQL_ROW row;
  while((row=mysql_fetch_row(r))){
    long bet_id=atol(row[0]); const char* market=row[1]; const char* side=row[2];
    double line=atof(row[3]); int is_asian=atoi(row[4]); double line_b=atof(row[5]);
    double price=atof(row[6]); double price_b=atof(row[7]); long long stake=atoll(row[8]); long runner_id=atol(row[9]);
    long long payout=0, profit=0; const char* result="lose";
    settle_compute(market,side,is_asian,line,line_b,price,price_b,stake,hs,as,&payout,&profit,&result);

    char up[512];
    snprintf(up,sizeof(up),
      "UPDATE bets SET status='settled', result='%s', payout_cents=%lld, profit_cents=%lld, settled_at=NOW() WHERE id=%ld",
      result, (long long)payout, (long long)profit, bet_id);
    if (db_exec(c,up)!=0){ mysql_free_result(r); return 5; }

    /* runner commission */
    char qrc[512];
    snprintf(qrc,sizeof(qrc),"SELECT commission_scheme,commission_rate FROM runners WHERE id=%ld", runner_id);
    if (db_exec(c,qrc)!=0){ mysql_free_result(r); return 5; }
    MYSQL_RES* rr = mysql_store_result(c);
    if (rr){
      MYSQL_ROW rw = mysql_fetch_row(rr);
      if (rw){
        const char* scheme=rw[0]?rw[0]:"net"; double rate=rw[1]?atof(rw[1]):10.0;
        long long comm=0;
        if (!strcmp(scheme,"handle")) {
          comm = (long long)(stake*(rate/100.0)+0.5);
        } else {
          long long net_for_book = -profit;
          long long base = net_for_book>0?net_for_book:0;
          comm = (long long)(base*(rate/100.0)+0.5);
        }
        char insc[512];
        snprintf(insc,sizeof(insc),
          "INSERT IGNORE INTO runner_commissions(bet_id,runner_id,commission_cents,scheme,rate) VALUES(%ld,%ld,%lld,'%s',%.2f)",
          bet_id, runner_id, (long long)comm, scheme, rate);
        if (db_exec(c,insc)!=0){ mysql_free_result(rr); mysql_free_result(r); return 5; }
      }
      mysql_free_result(rr);
    }
  }
  mysql_free_result(r);
  printf("OK settled event %ld\n", event);
  return 0;
}

/* ---------- REPORTS ---------- */

static int cmd_report(int argc, char** argv, MYSQL* c) {
  if (argc<2){
    fprintf(stderr,"report pnl|runner-commissions|bettor-balances|runner-balances [--format table|json|csv] [--out <file>]\n");
    return 2;
  }
  const char* sub=argv[1]; optind=1;

  long bm=0; const char* from=NULL; const char* to=NULL; const char* group=NULL;
  const char* out_path=NULL; rf_format_t fmt = RF_TABLE;

  static struct option o[]={
    {"bookmaker-id",1,0,'b'},
    {"from",1,0,'f'},
    {"to",1,0,'t'},
    {"by",1,0,'g'},
    {"format",1,0,'F'},
    {"out",1,0,'O'},
    {0,0,0,0}
  };
  int ch,ix=0;
  while((ch=getopt_long(argc-1,argv+1,"b:f:t:g:F:O:",o,&ix))!=-1){
    if(ch=='b') bm=atol(optarg);
    else if(ch=='f') from=optarg;
    else if(ch=='t') to=optarg;
    else if(ch=='g') group=optarg;
    else if(ch=='F') fmt = rf_format_from_str(optarg);
    else if(ch=='O') out_path=optarg;
    else return 2;
  }

  /* build query per subcommand and then print formatted */
  if (!strcmp(sub,"pnl")) {
    if(!bm||!from||!to){
      fprintf(stderr,"required: --bookmaker-id --from YYYY-MM-DD --to YYYY-MM-DD\n");
      return 2;
    }
    char q[1024];
    if (group && !strcmp(group,"runner")) {
      snprintf(q,sizeof(q),
        "SELECT r.id AS runner_id, r.name, COUNT(b.id) AS bets, ROUND(SUM(b.stake_cents)/100,2) AS handle_usd, ROUND(SUM(COALESCE(b.profit_cents,0))/100,2) AS profit_usd "
        "FROM bets b JOIN runners r ON r.id=b.runner_id "
        "WHERE b.bookmaker_id=%ld AND b.status='settled' AND b.settled_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND b.settled_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY) "
        "GROUP BY r.id,r.name ORDER BY profit_usd DESC", bm, from, to);
    } else if (group && !strcmp(group,"bettor")) {
      snprintf(q,sizeof(q),
        "SELECT bt.id AS bettor_id, bt.code, COUNT(b.id) AS bets, ROUND(SUM(b.stake_cents)/100,2) AS handle_usd, ROUND(SUM(COALESCE(b.profit_cents,0))/100,2) AS profit_usd "
        "FROM bets b JOIN bettors bt ON bt.id=b.bettor_id "
        "WHERE b.bookmaker_id=%ld AND b.status='settled' AND b.settled_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND b.settled_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY) "
        "GROUP BY bt.id,bt.code ORDER BY profit_usd ASC", bm, from, to);
    } else {
      snprintf(q,sizeof(q),
        "SELECT COUNT(id) AS bets, ROUND(SUM(stake_cents)/100,2) AS handle_usd, ROUND(SUM(COALESCE(profit_cents,0))/100,2) AS profit_usd "
        "FROM bets WHERE bookmaker_id=%ld AND status='settled' AND settled_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND settled_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY)",
        bm, from, to);
    }
    if (db_exec(c,q)!=0) { return 5; }
    MYSQL_RES* r = mysql_store_result(c);
    if (r) {
      int rc = print_formatted(r, fmt, out_path);
      mysql_free_result(r);
      return rc;
    }
    return 0;
  }

  if (!strcmp(sub,"runner-commissions")) {
    if(!bm||!from||!to){
      fprintf(stderr,"required: --bookmaker-id --from --to\n");
      return 2;
    }
    char q[1024];
    snprintf(q,sizeof(q),
      "SELECT r.id AS runner_id, r.name, ROUND(SUM(rc.commission_cents)/100,2) AS commissions_usd, COUNT(rc.id) AS items "
      "FROM runner_commissions rc JOIN bets b ON b.id=rc.bet_id JOIN runners r ON r.id=rc.runner_id "
      "WHERE b.bookmaker_id=%ld AND b.settled_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND b.settled_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY) "
      "GROUP BY r.id,r.name ORDER BY commissions_usd DESC", bm, from, to);
    if (db_exec(c,q)!=0) { return 5; }
    MYSQL_RES* r = mysql_store_result(c);
    if (r) {
      int rc = print_formatted(r, fmt, out_path);
      mysql_free_result(r);
      return rc;
    }
    return 0;
  }

  if (!strcmp(sub,"bettor-balances")) {
    if(!bm||!from||!to){
      fprintf(stderr,"required: --bookmaker-id --from --to\n");
      return 2;
    }
    char q[1024];
    snprintf(q,sizeof(q),
      "SELECT bt.id AS bettor_id, bt.code, ROUND(-SUM(COALESCE(b.profit_cents,0))/100,2) AS owed_gross_usd, "
      "ROUND(COALESCE((SELECT SUM(pr.amount_cents) FROM payouts_bettor pr WHERE pr.bettor_id=bt.id AND pr.created_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND pr.created_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY)),0)/100,2) AS paid_usd, "
      "ROUND((-SUM(COALESCE(b.profit_cents,0)) - COALESCE((SELECT SUM(pr.amount_cents) FROM payouts_bettor pr WHERE pr.bettor_id=bt.id AND pr.created_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND pr.created_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY)),0))/100,2) AS balance_usd "
      "FROM bets b JOIN bettors bt ON bt.id=b.bettor_id "
      "WHERE b.bookmaker_id=%ld AND b.status='settled' AND b.settled_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND b.settled_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY) "
      "GROUP BY bt.id,bt.code ORDER BY balance_usd DESC", from,to, from,to, bm, from,to);
    if (db_exec(c,q)!=0) { return 5; }
    MYSQL_RES* r = mysql_store_result(c);
    if (r) {
      int rc = print_formatted(r, fmt, out_path);
      mysql_free_result(r);
      return rc;
    }
    return 0;
  }

  if (!strcmp(sub,"runner-balances")) {
    if(!bm||!from||!to){
      fprintf(stderr,"required: --bookmaker-id --from --to\n");
      return 2;
    }
    char q[1024];
    snprintf(q,sizeof(q),
      "SELECT r.id AS runner_id, r.name, "
      "ROUND(COALESCE(SUM(rc.commission_cents),0)/100,2) AS commissions_usd, "
      "ROUND(COALESCE((SELECT SUM(pr.amount_cents) FROM payouts_runner pr WHERE pr.runner_id=r.id AND pr.created_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND pr.created_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY)),0)/100,2) AS paid_usd, "
      "ROUND((COALESCE(SUM(rc.commission_cents),0) - COALESCE((SELECT SUM(pr.amount_cents) FROM payouts_runner pr WHERE pr.runner_id=r.id AND pr.created_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND pr.created_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY)),0))/100,2) AS balance_usd "
      "FROM runners r LEFT JOIN runner_commissions rc ON rc.runner_id=r.id LEFT JOIN bets b ON b.id=rc.bet_id "
      "WHERE b.bookmaker_id=%ld AND b.settled_at>=STR_TO_DATE('%s','%%Y-%%m-%%d') AND b.settled_at<DATE_ADD(STR_TO_DATE('%s','%%Y-%%m-%%d'), INTERVAL 1 DAY) "
      "GROUP BY r.id,r.name ORDER BY balance_usd DESC", from,to, from,to, bm, from,to);
    if (db_exec(c,q)!=0) { return 5; }
    MYSQL_RES* r = mysql_store_result(c);
    if (r) {
      int rc = print_formatted(r, fmt, out_path);
      mysql_free_result(r);
      return rc;
    }
    return 0;
  }

  fprintf(stderr,"unknown report subcommand\n");
  return 2;
}

/* ---------- RISK ---------- */

static long long risk_delta(long long stake_cents, double price, int cmp) {
  if (cmp > 0) {
    long long loss = (long long)(stake_cents * (price - 1.0) + 0.5);
    return -loss;
  } else if (cmp == 0) {
    return 0;
  } else {
    return stake_cents;
  }
}

static int cmd_risk(int argc, char** argv, MYSQL* c) {
  if (argc<2 || strcmp(argv[1],"list")!=0){
    fprintf(stderr,"risk list --event-id X\n"); return 2;
  }
  long event=0; static struct option o[]={{"event-id",1,0,'e'},{0,0,0,0}}; int ch,ix=0; optind=1;
  while((ch=getopt_long(argc-1,argv+1,"e:",o,&ix))!=-1){
    if(ch=='e') event=atol(optarg); else return 2;
  }
  if(!event){
    fprintf(stderr,"required: --event-id\n");
    return 2;
  }

  const char* qf =
    "SELECT market_type,pick_side,COALESCE(line,0),is_asian,COALESCE(line_b,0),price_decimal,COALESCE(price_decimal_b,0),stake_cents "
    "FROM bets WHERE event_id=%ld AND status='open'";
  char qb[256]; snprintf(qb,sizeof(qb), qf, event);
  if (db_exec(c,qb)!=0) { return 5; }
  MYSQL_RES* r = mysql_store_result(c); if(!r){
    printf("Scenario\tExposure_USD\n");
    return 0;
  }

  long long ex_HOME=0, ex_AWAY=0, ex_DRAW=0, ex_OVER=0, ex_UNDER=0;
  MYSQL_ROW row;
  while((row=mysql_fetch_row(r))){
    const char* market=row[0]; const char* side=row[1];
    double line=atof(row[2]); int is_asian=atoi(row[3]); double line_b=atof(row[4]);
    double price=atof(row[5]); double price_b=atof(row[6]); long long stake=atoll(row[7]);
    (void)line_b; /* not used for exposure computation */

    if (!strcmp(market,"moneyline")||!strcmp(market,"threeway")) {
      int cmp_home = (!strcmp(side,"HOME"))? 1 : (!strcmp(side,"AWAY")? -1 : 0);
      int cmp_away = (!strcmp(side,"AWAY"))? 1 : (!strcmp(side,"HOME")? -1 : 0);
      int cmp_draw = (!strcmp(market,"threeway") && !strcmp(side,"DRAW")) ? 1 : -1;
      ex_HOME += risk_delta(stake, price, cmp_home);
      ex_AWAY += risk_delta(stake, price, cmp_away);
      if (!strcmp(market,"threeway")) ex_DRAW += risk_delta(stake, price, cmp_draw);
    } else if (!strcmp(market,"spread")) {
      if (!strcmp(side,"HOME")) {
        if (is_asian && (line-(int)line!=0.0)) {
          ex_HOME += risk_delta(stake/2, price,  1) + risk_delta(stake/2, (price_b>1.0?price_b:price),  1);
          ex_AWAY += risk_delta(stake/2, price, -1) + risk_delta(stake/2, (price_b>1.0?price_b:price), -1);
        } else {
          ex_HOME += risk_delta(stake, price,  1);
          ex_AWAY += risk_delta(stake, price, -1);
        }
      } else {
        if (is_asian && (line-(int)line!=0.0)) {
          ex_AWAY += risk_delta(stake/2, price,  1) + risk_delta(stake/2, (price_b>1.0?price_b:price),  1);
          ex_HOME += risk_delta(stake/2, price, -1) + risk_delta(stake/2, (price_b>1.0?price_b:price), -1);
        } else {
          ex_AWAY += risk_delta(stake, price,  1);
          ex_HOME += risk_delta(stake, price, -1);
        }
      }
    } else if (!strcmp(market,"total")) {
      if (is_asian && (line-(int)line!=0.0)) {
        int cmp_over  = (!strcmp(side,"OVER"))  ? 1 : -1;
        int cmp_under = (!strcmp(side,"UNDER")) ? 1 : -1;
        ex_OVER  += risk_delta(stake/2, price, cmp_over) + risk_delta(stake/2, (price_b>1.0?price_b:price), cmp_over);
        ex_UNDER += risk_delta(stake/2, price, cmp_under) + risk_delta(stake/2, (price_b>1.0?price_b:price), cmp_under);
      } else {
        int cmp_over  = (!strcmp(side,"OVER"))  ? 1 : -1;
        int cmp_under = (!strcmp(side,"UNDER")) ? 1 : -1;
        ex_OVER  += risk_delta(stake, price, cmp_over);
        ex_UNDER += risk_delta(stake, price, cmp_under);
      }
    }
  }
  mysql_free_result(r);

  printf("Scenario\tExposure_USD\n");
  printf("HOME_wins\t%.2f\n",  ex_HOME/100.0);
  printf("AWAY_wins\t%.2f\n",  ex_AWAY/100.0);
  printf("DRAW\t%.2f\n",       ex_DRAW/100.0);
  printf("OVER\t%.2f\n",       ex_OVER/100.0);
  printf("UNDER\t%.2f\n",      ex_UNDER/100.0);
  return 0;
}

/* ---------- DISPATCH ---------- */

int cli_dispatch(int argc, char** argv) {
  if (argc < 2) { usage_root(); return 1; }

  db_config_t cfg; db_load_env(&cfg);
  MYSQL* conn = db_connect(&cfg);
  if (!conn) { fprintf(stderr,"DB connect failed\n"); return 5; }

  int rc=2; const char* cmd = argv[1];
  if      (!strcmp(cmd,"sport"))     { rc = cmd_sport(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"league"))    { rc = cmd_league(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"team"))      { rc = cmd_team(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"bookmaker")) { rc = cmd_bookmaker(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"runner"))    { rc = cmd_runner(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"bettor"))    { rc = cmd_bettor(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"event"))     { rc = cmd_event(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"quote"))     { rc = cmd_quote(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"bet"))       { rc = cmd_bet(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"settle"))    { rc = cmd_settle(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"report"))    { rc = cmd_report(argc-1, argv+1, conn); }
  else if (!strcmp(cmd,"risk"))      { rc = cmd_risk(argc-1, argv+1, conn); }
  else { usage_root(); rc=1; }

  db_disconnect(conn);
  return rc;
}

