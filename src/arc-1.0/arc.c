/*************************************************************
** CrazyWWWBoard Mail Gateway Archiver                      **
**                                                          **
**  Official distribution site : http://cwb98.nobreak.com   **
**           Technical contact : nobreak@nobreak.com        **
**                                                          **
**                        Developed by 'Seung-young Kim'    **
**                        Last updated at July, 7, 1998     **
**                                                          **
**      Copyright (c) 1996-1998 Nobreak Technologies, Inc.. **
*************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include "gdbm.h"
#include "gdbm_lib.h"
#include "qDecoder.h"
#include "arc.h"
#include "arc_parse.h"
#include "arc_write.h"

Entry *getRealtimeConfig(SysConfig *sys, char *filename);
int   accessLog(char *filename);

int main(int argc, char **argv){
  char *db;
  SysConfig sys;
  DBArticle article;
  Entry *sysp;
  char buf[0xff], runPath[0xff];
  char sAccessLog[0xff], sErrorLog[0xff];
  int i;
  char *relPath;
  size_t len;

  if(argc != 2) qError("Usage: %s DB_NAME", OFFICIAL_GATEPRG_NAME);
  db = argv[1];

  /* db name is used in file paths : only 0-9, A-Z, a-z */
  if(!strcmp(db, "") || qStr09AZaz(db) == FALSE || strlen(db) > MAX_DBNAME_LEN) {
    qError("Only regular expression(0-9, A-Z, a-z) up to %d characters is allowed for the database name.", MAX_DBNAME_LEN);
  }

  /* Adjust umask */
  umask(S_IWGRP | S_IWOTH);

  /* 현재 실행 디렉토리 파악 */
  if(snprintf(runPath, sizeof(runPath), "%s", argv[0]) >= (int)sizeof(runPath)) qError("Path is too long.");
  if(strstr(runPath, "/") == NULL) strcpy(runPath, ".");
  else { /* /PATH/EXE -> /PATH */
    for(i = strlen(runPath) - 1; i >= 0 && runPath[i] != '/'; i--);
    if(i < 0) strcpy(runPath, "");
    else runPath[i] = '\0';
  }

  /* get realtime config */
  if(snprintf(buf, sizeof(buf), "%s/%s", runPath, REALTIME_CONFIG_FILE) >= (int)sizeof(buf)) qError("Path is too long.");
  sysp = getRealtimeConfig(&sys, buf);

  /* RealBasePath를 상대경로 지정했을때 이를 절대경로로 변경한다 */
  if(sys.RealBasePath[0] != '/') {
    relPath = sys.RealBasePath;
    len = strlen(runPath) + strlen("/") + strlen(relPath) + 1;
    if((sys.RealBasePath = (char *)malloc(len)) == NULL) qError("Memory allocation fail.");
    snprintf(sys.RealBasePath, len, "%s/%s", runPath, relPath);
  }

  /* default error log on : 쿼리 패치도중 발생하는 오류를 기록하기 위해*/
  if(strcmp(sys.LogDirectory, "")) {
    snprintf(sErrorLog, sizeof(sErrorLog), "%s/%s/error_log", sys.RealBasePath, sys.LogDirectory);
    qErrorLog(sErrorLog);
  }

  /* DB Check */
  if(snprintf(buf, sizeof(buf), "%s/%s/%s.%s", sys.RealBasePath, sys.DataDirectory, db, sys.DataExtension) >= (int)sizeof(buf)) qError("Path is too long.");
  if(qCheckFile(buf) == FALSE) qError("Database(%s) not found.", db);

  if(strcmp(sys.LogDirectory, "")) {
    snprintf(sAccessLog, sizeof(sAccessLog), "%s/%s/access_log.%s", sys.RealBasePath, sys.LogDirectory, db);
    snprintf(sErrorLog,  sizeof(sErrorLog),  "%s/%s/error_log.%s",  sys.RealBasePath, sys.LogDirectory, db);
    qErrorLog(sErrorLog);
    if(accessLog(sAccessLog) == FALSE) qError("Access logging(%s) fail.", sAccessLog);
  }

  if(arc_parse(&article) == FALSE) qError("Unstable email contents.");

  /* 루핑되지 않았으면 기록 */
  if(strncmp(article.Name, sys.MailName, strlen(sys.MailName))) arc_write(&sys, &article, db);

  qfFree(sysp);
  return 0;
}

Entry *getRealtimeConfig(SysConfig *sys, char *filename) {
  Entry *sysp;

  sysp = qfDecoder(filename);
  if(sysp == NULL) qError("Realtime configuration file(%s) not found.", filename);

#define FetchConf(NAME) if((sys->NAME = qfValue(sysp, #NAME)) == NULL) qError("There is no entry named '%s' in realtime configuration file.", #NAME)

  FetchConf(RealBasePath);

  FetchConf(DataDirectory);
  FetchConf(DataExtension);

  FetchConf(LogDirectory);

  FetchConf(MailName);

#undef FetchConf

  return sysp;
}


int accessLog(char *filename) {
  FILE *fp;
  char *http_user_agent, *remote_host, *mode, *num;
  struct tm *time;

  time = qGetTime();
  http_user_agent = OFFICIAL_GATEPRG_NAME;
  remote_host     = "localhost";
  mode            = "archive";
  num             = "";

  if((fp = fopen(filename, "at")) == NULL) return FALSE;
  fprintf(fp, "%04d/%02d/%02d(%02d:%02d) mode=%-10s num=%-5s from %s (%s)\n", time->tm_year, time->tm_mon, time->tm_mday, time->tm_hour, time->tm_min, mode, num, remote_host, http_user_agent);
  fclose(fp);

  return TRUE;
}
