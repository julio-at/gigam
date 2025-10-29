#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

static void usage_root(void) {
  fprintf(stderr,
    "gigamctl (PoC)\n"
    "Commands:\n"
    "  bookmaker create|list\n"
    "  runner    create|list|set-default\n"
    "  bettor    create|list\n"
    "  event     create|list|set-score\n"
    "  quote     add|list\n"
    "  bet       place|list\n"
  );
}

static int exec_and_print(MYSQL* c, const char* sql) {
  if (db_exec(c, sql) != 0) return -1;
  MYSQL_RES* r = mysql_store_result(c);
  if (r) { db_print_result(r); mysql_free_result(r); }
  return 0;
}

static void esc_str(MYSQL* c, const char* in, char* out, size_t outsz) {
  if (!in) { out[0]='\0'; return; }
  size_t n = strlen(in);
  char* tmp = malloc(n*2 + 1);
  if (!tmp) { out[0]='\0'; return; }
  unsigned long m = mysql_real_escape_string(c, tmp, in, n);
  snprintf(out, outsz, "%.*s", (int)m, tmp);
  free(tmp);
}

/* --- bookmaker --- */
static int cmd_bookmaker(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr, "bookmaker create|list\n"); return 2; }
  const char* sub = argv[1]; optind = 1;

  if (strcmp(sub,"create")==0) {
    const char *name=NULL, *currency="USD";
    static struct option o[] = { {"name",1,0,'n'}, {"currency",1,0,'c'}, {0,0,0,0} };
    int ch,ix=0;
    while ((ch=getopt_long(argc-1, argv+1, "n:c:", o, &ix)) != -1) {
      if (ch=='n') name=optarg; else if (ch=='c') currency=optarg; else return 2;
    }
    if (!name) { fprintf(stderr, "required: --name\n"); return 2; }
    char namee[256], cure[16];
    esc_str(c, name, namee, sizeof(namee));
    esc_str(c, currency, cure, sizeof(cure));
    char q[512];
    snprintf(q,sizeof(q),
      "INSERT INTO bookmakers(name,currency) VALUES('%s','%s') "
      "ON DUPLICATE KEY UPDATE currency=VALUES(currency)", namee, cure);
    if (db_exec(c,q)!=0) return 5;
    printf("OK\n"); return 0;
  }

  if (strcmp(sub,"list")==0) {
    return exec_and_print(c, "SELECT id,name,currency,created_at FROM bookmakers ORDER BY id");
  }

  fprintf(stderr,"unknown bookmaker subcommand\n"); return 2;
}

/* --- runner --- */
static int cmd_runner(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr, "runner create|list|set-default\n"); return 2; }
  const char* sub = argv[1]; optind = 1;

  if (strcmp(sub,"create")==0) {
    long bm=0; const char* user=NULL; const char* name=NULL; int is_def=0;
    static struct option o[] = {
      {"bookmaker-id",1,0,'b'}, {"user",1,0,'u'}, {"name",1,0,'n'}, {"default",0,0,'d'}, {0,0,0,0}
    };
    int ch,ix=0;
    while ((ch=getopt_long(argc-1, argv+1, "b:u:n:d", o, &ix)) != -1) {
      if (ch=='b') bm=atol(optarg);
      else if (ch=='u') user=optarg;
      else if (ch=='n') name=optarg;
      else if (ch=='d') is_def=1;
      else return 2;
    }
    if (!bm || !user || !name) { fprintf(stderr,"required: --bookmaker-id --user --name\n"); return 2; }

    char ue[256], ne[256];
    esc_str(c, user, ue, sizeof(ue));
    esc_str(c, name, ne, sizeof(ne));

    char q1[512];
    snprintf(q1,sizeof(q1),
      "INSERT INTO users(username,display_name) VALUES('%s','%s') "
      "ON DUPLICATE KEY UPDATE display_name=VALUES(display_name)", ue, ne);
    if (db_exec(c,q1)!=0) return 5;

    char q2[512];
    snprintf(q2,sizeof(q2),
      "INSERT INTO runners(user_id,bookmaker_id,name,is_default) "
      "SELECT id,%ld,'%s',%d FROM users WHERE username='%s' "
      "ON DUPLICATE KEY UPDATE name=VALUES(name), is_default=VALUES(is_default)",
      bm, ne, is_def, ue);
    if (db_exec(c,q2)!=0) return 5;

    printf("OK\n");
    return 0;
  }

  if (strcmp(sub,"list")==0) {
    long bm=0; static struct option o[]={{"bookmaker-id",1,0,'b'},{0,0,0,0}};
    int ch,ix=0; while ((ch=getopt_long(argc-1,argv+1,"b:",o,&ix))!=-1) { if(ch=='b') bm=atol(optarg); else return 2; }
    if (!bm) { fprintf(stderr,"required: --bookmaker-id\n"); return 2; }
    char q[256];
    snprintf(q,sizeof(q),
      "SELECT r.id,r.name,r.is_default,u.username FROM runners r "
      "JOIN users u ON u.id=r.user_id WHERE r.bookmaker_id=%ld ORDER BY r.id", bm);
    return exec_and_print(c,q);
  }

  if (strcmp(sub,"set-default")==0) {
    long bm=0, rid=0; static struct option o[]={{"bookmaker-id",1,0,'b'},{"runner-id",1,0,'r'},{0,0,0,0}};
    int ch,ix=0; while ((ch=getopt_long(argc-1,argv+1,"b:r:",o,&ix))!=-1) { if(ch=='b') bm=atol(optarg); else if(ch=='r') rid=atol(optarg); else return 2; }
    if (!bm || !rid) { fprintf(stderr,"required: --bookmaker-id --runner-id\n"); return 2; }
    char q1[128]; snprintf(q1,sizeof(q1),"UPDATE runners SET is_default=0 WHERE bookmaker_id=%ld", bm);
    if (db_exec(c,q1)!=0) return 5;
    char q2[192]; snprintf(q2,sizeof(q2),"UPDATE runners SET is_default=1 WHERE id=%ld AND bookmaker_id=%ld", rid, bm);
    if (db_exec(c,q2)!=0) return 5;
    printf("OK\n");
    return 0;
  }

  fprintf(stderr,"unknown runner subcommand\n"); return 2;
}

/* --- bettor --- */
static int cmd_bettor(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr, "bettor create|list\n"); return 2; }
  const char* sub = argv[1]; optind=1;

  if (strcmp(sub,"create")==0) {
    long runner=0; const char* code=NULL; const char* name="";
    static struct option o[]={{"runner-id",1,0,'r'},{"code",1,0,'c'},{"name",1,0,'n'},{0,0,0,0}};
    int ch,ix=0; while((ch=getopt_long(argc-1,argv+1,"r:c:n:",o,&ix))!=-1){
      if(ch=='r') runner=atol(optarg); else if(ch=='c') code=optarg; else if(ch=='n') name=optarg; else return 2;
    }
    if (!runner || !code) { fprintf(stderr,"required: --runner-id --code [--name]\n"); return 2; }
    char ce[128], ne[256]; esc_str(c, code, ce, sizeof(ce)); esc_str(c, name, ne, sizeof(ne));
    char q[512]; snprintf(q,sizeof(q),
      "INSERT INTO bettors(runner_id,code,display_name) VALUES(%ld,'%s','%s') "
      "ON DUPLICATE KEY UPDATE display_name=VALUES(display_name)", runner, ce, ne);
    if (db_exec(c,q)!=0) return 5; printf("OK\n"); return 0;
  }

  if (strcmp(sub,"list")==0) {
    long runner=0; static struct option o[]={{"runner-id",1,0,'r'},{0,0,0,0}};
    int ch,ix=0; while((ch=getopt_long(argc-1,argv+1,"r:",o,&ix))!=-1){ if(ch=='r') runner=atol(optarg); else return 2; }
    if (!runner) { fprintf(stderr,"required: --runner-id\n"); return 2; }
    char q[256]; snprintf(q,sizeof(q),
      "SELECT id,code,display_name,created_at FROM bettors WHERE runner_id=%ld ORDER BY id", runner);
    return exec_and_print(c,q);
  }

  fprintf(stderr,"unknown bettor subcommand\n"); return 2;
}

/* --- event --- */
static int cmd_event(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr,"event create|list|set-score\n"); return 2; }
  const char* sub = argv[1]; optind=1;

  if (strcmp(sub,"create")==0) {
    long league=0, home=0, away=0; const char* starts=NULL;
    static struct option o[]={{"league-id",1,0,'l'},{"starts-at",1,0,'t'},{"home-id",1,0,'h'},{"away-id",1,0,'a'},{0,0,0,0}};
    int ch,ix=0; while((ch=getopt_long(argc-1,argv+1,"l:t:h:a:",o,&ix))!=-1){
      if(ch=='l') league=atol(optarg); else if(ch=='t') starts=optarg; else if(ch=='h') home=atol(optarg); else if(ch=='a') away=atol(optarg); else return 2;
    }
    if (!league||!home||!away||!starts) { fprintf(stderr,"required: --league-id --starts-at --home-id --away-id\n"); return 2; }
    char se[64]; esc_str(c, starts, se, sizeof(se));
    char q[512]; snprintf(q,sizeof(q),
      "INSERT INTO events(league_id,starts_at,home_team_id,away_team_id) VALUES(%ld,'%s',%ld,%ld)",
      league, se, home, away);
    if (db_exec(c,q)!=0) return 5; printf("OK\n"); return 0;
  }

  if (strcmp(sub,"list")==0) {
    return exec_and_print(c,
      "SELECT id,league_id,DATE_FORMAT(starts_at,'%Y-%m-%d %H:%i') AS starts_at,home_team_id,away_team_id,status "
      "FROM events ORDER BY starts_at");
  }

  if (strcmp(sub,"set-score")==0) {
    long event=0; int home=0, away=0; int fin=0;
    static struct option o[]={{"event-id",1,0,'e'},{"home",1,0,'h'},{"away",1,0,'a'},{"final",0,0,'f'},{0,0,0,0}};
    int ch,ix=0; while((ch=getopt_long(argc-1,argv+1,"e:h:a:f",o,&ix))!=-1){
      if(ch=='e') event=atol(optarg); else if(ch=='h') home=atoi(optarg); else if(ch=='a') away=atoi(optarg); else if(ch=='f') fin=1; else return 2;
    }
    if (!event) { fprintf(stderr,"required: --event-id\n"); return 2; }
    char q[256]; snprintf(q,sizeof(q),
      "UPDATE events SET home_score=%d, away_score=%d%s WHERE id=%ld",
      home, away, fin?", status='final'":"", event);
    if (db_exec(c,q)!=0) return 5; printf("OK\n"); return 0;
  }

  fprintf(stderr,"unknown event subcommand\n"); return 2;
}

/* --- quote --- */
static int cmd_quote(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr,"quote add|list\n"); return 2; }
  const char* sub=argv[1]; optind=1;

  if (strcmp(sub,"add")==0) {
    long event=0, bm=0; const char* market=NULL; const char* side=NULL; double line=0.0, line_b=0.0, price=0.0, price_b=0.0; int asian=0;
    static struct option o[]={
      {"event-id",1,0,'e'},{"bookmaker-id",1,0,'b'},{"market",1,0,'m'},{"side",1,0,'s'},
      {"line",1,0,'l'},{"price",1,0,'p'},{"asian",0,0,'a'},{"line-b",1,0,'L'},{"price-b",1,0,'P'},{0,0,0,0}};
    int ch,ix=0; while((ch=getopt_long(argc-1,argv+1,"e:b:m:s:l:p:aL:P:",o,&ix))!=-1){
      if(ch=='e') event=atol(optarg); else if(ch=='b') bm=atol(optarg); else if(ch=='m') market=optarg; else if(ch=='s') side=optarg;
      else if(ch=='l') line=atof(optarg); else if(ch=='p') price=atof(optarg); else if(ch=='a') asian=1; else if(ch=='L') line_b=atof(optarg); else if(ch=='P') price_b=atof(optarg); else return 2;
    }
    if (!event || !bm || !market || !side || price<=1.0) { fprintf(stderr,"required: --event-id --bookmaker-id --market --side --price\n"); return 2; }
    if (asian && (line_b==0.0 || price_b<=1.0)) { fprintf(stderr,"asian requires: --line-b --price-b\n"); return 2; }
    char me[32], se[16];
    esc_str(c, market, me, sizeof(me));
    esc_str(c, side, se, sizeof(se));
    char line_sql[32], lineb_sql[32], priceb_sql[32];
    if (!strcmp(me,"moneyline") || !strcmp(me,"threeway")) snprintf(line_sql,sizeof(line_sql),"NULL");
    else snprintf(line_sql,sizeof(line_sql),"%.2f", line);
    if (asian) { snprintf(lineb_sql,sizeof(lineb_sql),"%.2f", line_b); snprintf(priceb_sql,sizeof(priceb_sql),"%.4f", price_b); }
    else { snprintf(lineb_sql,sizeof(lineb_sql),"NULL"); snprintf(priceb_sql,sizeof(priceb_sql),"NULL"); }
    char q[512];
    snprintf(q,sizeof(q),
      "INSERT INTO quotes(event_id,bookmaker_id,market_type,side,line,is_asian,line_b,price_decimal,price_decimal_b) "
      "VALUES(%ld,%ld,'%s','%s',%s,%d,%s,%.4f,%s)",
      event,bm,me,se,line_sql,asian,lineb_sql,price,priceb_sql);
    if (db_exec(c,q)!=0) return 5; printf("OK\n"); return 0;
  }

  if (strcmp(sub,"list")==0) {
    long event=0, bm=0; static struct option o[]={{"event-id",1,0,'e'},{"bookmaker-id",1,0,'b'},{0,0,0,0}};
    int ch,ix=0; while((ch=getopt_long(argc-1,argv+1,"e:b:",o,&ix))!=-1){ if(ch=='e') event=atol(optarg); else if(ch=='b') bm=atol(optarg); else return 2; }
    if (!event || !bm) { fprintf(stderr,"required: --event-id --bookmaker-id\n"); return 2; }
    char q[512]; snprintf(q,sizeof(q),
      "SELECT id,market_type,side,COALESCE(CAST(line AS CHAR),'-') line,is_asian,COALESCE(CAST(line_b AS CHAR),'-') line_b,price_decimal,COALESCE(CAST(price_decimal_b AS CHAR),'-') price_b,DATE_FORMAT(captured_at,'%%Y-%%m-%%d %%H:%%i:%%s') captured_at "
      "FROM quotes WHERE event_id=%ld AND bookmaker_id=%ld ORDER BY id DESC", event, bm);
    return exec_and_print(c,q);
  }

  fprintf(stderr,"unknown quote subcommand\n"); return 2;
}

/* --- bet --- */
static int cmd_bet(int argc, char** argv, MYSQL* c) {
  if (argc < 2) { fprintf(stderr,"bet place|list\n"); return 2; }
  const char* sub=argv[1]; optind=1;

  if (strcmp(sub,"place")==0) {
    long bm=0,event=0,runner=0,bettor=0; const char* market=NULL; const char* side=NULL;
    double line=0.0, price=0.0, line_b=0.0, price_b=0.0; long stake=0; int asian=0;
    static struct option o[]={
      {"bookmaker-id",1,0,'b'},{"event-id",1,0,'e'},{"runner-id",1,0,'r'},{"bettor-id",1,0,'t'},
      {"market",1,0,'m'},{"side",1,0,'s'},{"line",1,0,'l'},{"price",1,0,'p'},{"stake",1,0,'k'},
      {"asian",0,0,'a'},{"line-b",1,0,'L'},{"price-b",1,0,'P'},{0,0,0,0}};
    int ch,ix=0; while((ch=getopt_long(argc-1,argv+1,"b:e:r:t:m:s:l:p:k:aL:P:",o,&ix))!=-1){
      if(ch=='b') bm=atol(optarg); else if(ch=='e') event=atol(optarg); else if(ch=='r') runner=atol(optarg); else if(ch=='t') bettor=atol(optarg);
      else if(ch=='m') market=optarg; else if(ch=='s') side=optarg; else if(ch=='l') line=atof(optarg); else if(ch=='p') price=atof(optarg);
      else if(ch=='k') stake=atol(optarg); else if(ch=='a') asian=1; else if(ch=='L') line_b=atof(optarg); else if(ch=='P') price_b=atof(optarg); else return 2;
    }
    if(!bm||!event||!runner||!bettor||!market||!side||price<=1.0||stake<=0){
      fprintf(stderr,"required: --bookmaker-id --event-id --runner-id --bettor-id --market --side --price --stake\n"); return 2;
    }
    char me[32], se[16]; esc_str(c, market, me, sizeof(me)); esc_str(c, side, se, sizeof(se));
    char line_sql[32], priceb_sql[32];
    if(!strcmp(me,"moneyline")||!strcmp(me,"threeway")) snprintf(line_sql,sizeof(line_sql),"NULL"); else snprintf(line_sql,sizeof(line_sql),"%.2f",line);
    if(asian) snprintf(priceb_sql,sizeof(priceb_sql),"%.4f",price_b); else snprintf(priceb_sql,sizeof(priceb_sql),"NULL");

    char qset[512];
    if(!strcmp(me,"moneyline")||!strcmp(me,"threeway"))
      snprintf(qset,sizeof(qset),
        "SET @qid := (SELECT id FROM quotes WHERE event_id=%ld AND bookmaker_id=%ld AND market_type='%s' AND side='%s' ORDER BY id DESC LIMIT 1);",
        event,bm,me,se);
    else
      snprintf(qset,sizeof(qset),
        "SET @qid := (SELECT id FROM quotes WHERE event_id=%ld AND bookmaker_id=%ld AND market_type='%s' AND side='%s' AND line=%.2f ORDER BY id DESC LIMIT 1);",
        event,bm,me,se,line);
    if (db_exec(c,qset)!=0) return 5;

    char ins[768];
    snprintf(ins,sizeof(ins),
      "INSERT INTO bets(bookmaker_id,event_id,quote_id,stake_cents,market_type,pick_side,line,is_asian,price_decimal,runner_id,bettor_id,status,price_decimal_b) "
      "VALUES(%ld,%ld,@qid,%ld,'%s','%s',%s,%d,%.4f,%ld,%ld,'open',%s)",
      bm,event,stake,me,se,line_sql,asian,price,runner,bettor,priceb_sql);

    if (db_exec(c,ins)!=0) return 5;
    printf("OK\n"); return 0;
  }

  if (strcmp(sub,"list")==0) {
    long bm=0; static struct option o[]={{"bookmaker-id",1,0,'b'},{0,0,0,0}};
    int ch,ix=0; while((ch=getopt_long(argc-1,argv+1,"b:",o,&ix))!=-1){ if(ch=='b') bm=atol(optarg); else return 2; }
    if (!bm) { fprintf(stderr,"required: --bookmaker-id\n"); return 2; }
    char q[768];
    snprintf(q,sizeof(q),
      "SELECT b.id AS bet_id, DATE_FORMAT(b.placed_at,'%%Y-%%m-%%d %%H:%%i:%%s') placed_at, "
      "b.market_type,b.pick_side,COALESCE(CAST(b.line AS CHAR),'-') line,b.is_asian,b.price_decimal,ROUND(b.stake_cents/100,2) stake_usd "
      "FROM bets b WHERE b.bookmaker_id=%ld ORDER BY b.id DESC LIMIT 200", bm);
    return exec_and_print(c,q);
  }

  fprintf(stderr,"unknown bet subcommand\n"); return 2;
}

/* --- dispatch --- */
int cli_dispatch(int argc, char** argv) {
  if (argc < 2) { usage_root(); return 1; }

  db_config_t cfg; db_load_env(&cfg);
  MYSQL* conn = db_connect(&cfg);
  if (!conn) { fprintf(stderr,"DB connect failed\n"); return 5; }

  int rc=2;
  const char* cmd = argv[1];

  if      (!strcmp(cmd,"bookmaker")) rc = cmd_bookmaker(argc-1, argv+1, conn);
  else if (!strcmp(cmd,"runner"))    rc = cmd_runner(argc-1, argv+1, conn);
  else if (!strcmp(cmd,"bettor"))    rc = cmd_bettor(argc-1, argv+1, conn);
  else if (!strcmp(cmd,"event"))     rc = cmd_event(argc-1, argv+1, conn);
  else if (!strcmp(cmd,"quote"))     rc = cmd_quote(argc-1, argv+1, conn);
  else if (!strcmp(cmd,"bet"))       rc = cmd_bet(argc-1, argv+1, conn);
  else usage_root();

  db_disconnect(conn);
  return rc;
}
