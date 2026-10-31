#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>        /* GCC일경우에 활성화*/
/* #include <sys/dir.h> */ /* CC일경우에 활성화*/
#include "gdbm.h"
#include "gdbm_lib.h"
#include "qDecoder.h"
#include "adm.h"
#include "adm_list.h"

#define MB			(1024 * 1024)
#define INIT_STARTED_DATE	"999999"

typedef struct {
  char DBname[0xff];
  char *Type;
  int  TotalArticles;
  int  TodayArticles;
  int  UploadSize;
  int  TotalHits;
  int  HitsDay;
  int  TodayHits;
  char Started[0xff];

  char *Password;
}ADMList;

ADMList *getList(SysConfig *sys, int *tcount);
int  getDBname(char *dbname, int size, char *filename, char *ext);
void fetchDBinfo(char *dbDir, char *statDir, ADMList *info);

void adm_list(SysConfig *sys, DB *db) {
  ADMList *info, total;
  int i, count;
  char warm_rmlog[0xff], warm_rmstat[0xff], warm_rmdb[0xff];
  int  access_logExist, error_logExist, statExist;
  char buf[0xff];
  char *dbpass, *prog;

  /* clear */
  total.TotalArticles = 0;
  total.TodayArticles = 0;
  total.UploadSize    = 0;
  total.TotalHits     = 0;
  total.HitsDay       = 0;
  total.TodayHits     = 0;
  strcpy(total.Started, INIT_STARTED_DATE);

  info = getList(sys, &count);
  if(count > LICENSED_DB_AMOUNT) qError("There are too many(%d) databases. You are only licensed %d databases. Do not use manually.", count, LICENSED_DB_AMOUNT);

  headHtml(sys);

  printf("<table border='1' cellspacing=0 width='%s'>\n", LIST_WIDTH);
  printf("  <tr bgcolor=#fffacd>\n");
  printf("    <td align=center bgcolor=#4c81ce rowspan=2><font size=2 face='Arial'><b>DB Name</b></font></td>\n");
  printf("    <td align=center bgcolor=#4c81ce rowspan=2><font size=2 face='Arial'><b>Type</b></font></td>\n");
  printf("    <td align=center bgcolor=#4c81ce colspan=2><font size=2 face='Arial'><b>Articles</b></font></td>\n");
  printf("    <td align=center bgcolor=#4c81ce colspan=3><font size=2 face='Arial'><b>Statistics</b></font></td>\n");
  printf("    <td align=center bgcolor=#4c81ce rowspan=2><font size=2 face='Arial'><b>Options</b></font></td>\n");
  printf("    <td align=center bgcolor=#4c81ce rowspan=2><font size=2 face='Arial' color=red><b>Remove</b></font></td>\n");
  printf("  </tr>\n");
  printf("  <tr bgcolor=#fffacd>\n");
  printf("    <td align=center bgcolor=#cdf1ff><font size=2 face='Arial'>Total</font></td>\n");
  printf("    <td align=center bgcolor=#cdf1ff><font size=2 face='Arial'>Upload</font></td>\n");
  printf("    <td align=center bgcolor=#cdf1ff><font size=2 face='Arial'>Started</font></td>\n");
  printf("    <td align=center bgcolor=#cdf1ff><font size=2 face='Arial'>Total</font></td>\n");
  printf("    <td align=center bgcolor=#cdf1ff><font size=2 face='Arial'>Avg</font></td>\n");
  printf("  </tr>\n");

  for(i = 1; i <= count; i++) {
    /* sum */
    total.TotalArticles += info[i].TotalArticles;
    total.TodayArticles += info[i].TodayArticles;
    total.UploadSize    += info[i].UploadSize;
    total.TotalHits     += info[i].TotalHits;
    total.HitsDay       += info[i].HitsDay;
    total.TodayHits     += info[i].TodayHits;
    if((strcmp(total.Started, info[i].Started) > 0)) strcpy(total.Started, info[i].Started);

    snprintf(warm_rmstat, sizeof(warm_rmstat), "Do you want to CLEAR STATISTICS for %s ?", info[i].DBname);
    snprintf(warm_rmlog,  sizeof(warm_rmlog),  "Do you want to CLEAR LOG FILE for %s ?", info[i].DBname);
    snprintf(warm_rmdb,   sizeof(warm_rmdb),   "Do you want to REMOVE DATABASE named %s ?", info[i].DBname);

    snprintf(buf, sizeof(buf), "%s/%s/access_log.%s", sys->RealBasePath, sys->LogDirectory, info[i].DBname);
    access_logExist = qCheckFile(buf);

    snprintf(buf, sizeof(buf), "%s/%s/error_log.%s", sys->RealBasePath, sys->LogDirectory, info[i].DBname);
    error_logExist = qCheckFile(buf);

    snprintf(buf, sizeof(buf), "%s/%s/%s.%s", sys->RealBasePath, sys->StatDirectory, info[i].DBname, sys->StatExtension);
    statExist = qCheckFile(buf);

    printf("  <tr bgcolor=#f0f9ff><!-- DB Name '%s' -->\n", info[i].DBname);
    printf("    <td align=center><font size=2 face='Arial'><b>%s</b></font></td>\n", info[i].DBname);
    printf("    <td align=center><font size=2 face='Arial'>%s</font></td>\n", info[i].Type);
    printf("    <td align=right><font size=2 face='Arial'>%d/%d</font></td>\n", info[i].TodayArticles, info[i].TotalArticles);
    printf("    <td align=right><font size=2 face='Arial'>%.1fM</font></td>\n", (float)((float)info[i].UploadSize / (float)MB));
    printf("    <td align=center><font size=2 face='Arial'>%s</font></td>\n", info[i].Started);
    printf("    <td align=right><font size=2 face='Arial'>%d/%d</font></td>\n", info[i].TodayHits, info[i].TotalHits);
    printf("    <td align=right><font size=2 face='Arial'>%d</font></td>\n", info[i].HitsDay);
    printf("    <td align=left><table border=0 cellspacing=0 cellpadding=0><tr>\n");
    /* password is POSTed, not put into URLs */
    prog   = qHtmlEncode(sys->ProgramName);
    dbpass = qHtmlEncode(info[i].Password);
    printf("      <form method='post' action='%s'><td align=center><font size=2><input type=hidden name='db' value='%s'><input type=hidden name='mode' value='list'><input type=hidden name='password' value='%s'><input type=button value='VIEW' onClick=\"forkPost('view', this.form)\"></font></td></form>\n", prog, info[i].DBname, dbpass);
    printf("      <form method='post' action='%s'><td align=center><font size=2><input type=hidden name='db' value='%s'><input type=hidden name='mode' value='admin'><input type=hidden name='password' value='%s'><input type=button value='CONF' onClick=\"forkPost('conf', this.form)\"></font></td></form>\n", prog, info[i].DBname, dbpass);
    printf("      <form method='post' action='%s'><td align=center><font size=2><input type=hidden name='db' value='%s'><input type=hidden name='mode' value='sync'><input type=hidden name='password' value='%s'><input type=button value='SYNC' onClick=\"forkPost('sync', this.form)\"></font></td></form>\n", prog, info[i].DBname, dbpass);
    if(statExist) {
      printf("      <form method='post' action='%s'><td align=center><font size=2><input type=hidden name='db' value='%s'><input type=hidden name='mode' value='stat'><input type=hidden name='password' value='%s'><input type=button value='STAT' onClick=\"forkPost('stat', this.form)\"></font></td></form>\n", prog, info[i].DBname, dbpass);
    }
    free(prog);
    free(dbpass);
    if(access_logExist) {
      printf("      <form><td align=center><font size=2><input type=button value='LOG' onClick=\"fork('log', '%s/%s/access_log.%s')\"></font></td></form>\n", sys->HtmlBasePath, sys->LogDirectory, info[i].DBname);
    }
    printf("    </tr></table></td>\n");
    printf("    <td align=left><table border=0 cellspacing=0 cellpadding=0><tr>\n");
    printf("      <form method='post' action='%s'><td align=center><font size=2><input type=hidden name='db' value='%s'><input type=hidden name='mode' value='rm_db' ><input type=hidden name='password' value='%s'><input type=submit value='DB' onClick=\"term();return confirm('%s');\"></font></td></form>\n", sys->AdmPrgName, info[i].DBname, db->password, warm_rmdb);
    if(statExist) {
      printf("      <form method='post' action='%s'><td align=center><font size=2><input type=hidden name='db' value='%s'><input type=hidden name='mode' value='rm_stat'><input type=hidden name='password' value='%s'><input type=submit value='STAT' onClick=\"term();return confirm('%s');\"></font></td></form>\n", sys->AdmPrgName, info[i].DBname, db->password, warm_rmstat);
    }
    if(access_logExist || error_logExist) {
      printf("      <form method='post' action='%s'><td align=center><font size=2><input type=hidden name='db' value='%s'><input type=hidden name='mode' value='rm_log'><input type=hidden name='password' value='%s'><input type=submit value='LOG' onClick=\"term();return confirm('%s');\"></font></td></form>\n", sys->AdmPrgName, info[i].DBname, db->password, warm_rmlog);
    }
    printf("    </tr></table></td>\n");
    printf("  </tr>\n");
  }

  if(!strcmp(total.Started, INIT_STARTED_DATE)) strcpy(total.Started, "<font color=red>unknown</font>");

  /* total */
  printf("  <tr bgcolor=#fffacd>\n");
  printf("    <td align=center bgcolor=#cdf1ff colspan=2><font size=2 face='Arial'><b>TOTAL</b></font></td>\n");
  printf("    <td align=right  bgcolor=#cdf1ff><font size=2 face='Arial'>%d/%d</font></td>\n", total.TodayArticles, total.TotalArticles);
  printf("    <td align=right  bgcolor=#cdf1ff><font size=2 face='Arial'>%.1fM</font></td>\n", (float)((float)total.UploadSize / (float)MB));
  printf("    <td align=center bgcolor=#cdf1ff><font size=2 face='Arial'>%s</font></td>\n", total.Started);
  printf("    <td align=right  bgcolor=#cdf1ff><font size=2 face='Arial'>%d/%d</font></td>\n", total.TodayHits, total.TotalHits);
  printf("    <td align=right  bgcolor=#cdf1ff><font size=2 face='Arial'>%d</font></td>\n", total.HitsDay);
  printf("    <td align=right  bgcolor=#cdf1ff colspan=2 rowspan=2>\n");
  printf("      <table border=0>\n");
  printf("        <tr>\n");
  printf("          <td align=center>\n");
  printf("          <font size=2 face='Arial'>\n");
  printf("          <font color=red><b>P</b></font>owered by <font color=#8a3400><b><nobr>Nobreak Technologies, Inc.</nobr></b></font><br>\n");
  printf("          <a href='http://cwb98.nobreak.com' target='_top'>http://cwb98.nobreak.com</a>\n");
  printf("          </font>\n");
  printf("          </td>\n");

  /* reload */
  printf("          <form method='post' action='%s'>\n", sys->AdmPrgName);
  printf("          <input type=hidden name='password' value='%s'>\n", db->password);
  printf("          <td align=center colspan=2><font size=2 face='Arial'>\n");
  printf("          <input type=image name='submit' src='%s/%s' alt='Reload' border=0 onClick=\"term()\">\n", sys->HtmlBasePath, sys->IconReload);
  printf("          </font></td>\n");
  printf("          </form>\n");

  printf("        </tr>\n");
  printf("      </table>\n");
  printf("    </td>\n");
  printf("  </tr>\n");

  /* new db */
  printf("  <tr bgcolor=#93a7f5>\n");
  if(count < LICENSED_DB_AMOUNT) {
    printf("    <form method='post' action='%s'>\n", sys->AdmPrgName);
    printf("      <input type=hidden name='mode'     value='create'>\n");
    printf("      <input type=hidden name='password' value='%s'>\n", db->password);
    printf("    <td align=center colspan=2>\n");
    printf("      <input type=text   name='db' size=10 maxlength=20>\n");
    printf("    </td>\n");
    printf("    <td align=left colspan=5>\n");
    printf("      <font size=2><input type=submit value='CREATE NEW DATABASE' onClick=\"term()\"></font>\n");
    printf("    </td>\n");
    printf("    </form>\n");
  }
  else {
    printf("    <th colspan=2><font size=2 color=red>FULL</font></th>\n");
    printf("    <th colspan=5><font size=2>You are licensed %d databases</font></th>\n", LICENSED_DB_AMOUNT);
  }
  printf("  </tr>\n");
  printf("</table>\n");

  printf("<table border='0' cellspacing=0 width='%s'>\n", LIST_WIDTH);
  printf("  <tr>\n");

  /* change password */
  printf("    <form method='post' action='%s'><td align=right><font size=2>", sys->AdmPrgName);
  printf("      <input type=hidden name='mode'     value='newpass'>\n");
  printf("      <input type=hidden name='password' value='%s'>\n", db->password);
  printf("      <input type=submit value='CHANGE PASSWORD' onClick=\"term()\">\n");
  printf("    </font></td></form>\n");

  /* modify messages */
  printf("    <form method='post' action='%s'>\n", sys->AdmPrgName);
  printf("    <td align=center><font size=2><input type=hidden name='mode' value='message'><input type=hidden name='password' value='%s'><input type=button value='MODIFY MESSAGES' onClick=\"forkPost('message', this.form)\"></font></td>\n", db->password);
  printf("    </td></form>\n");

  /* global configuration */
  printf("    <form method='post' action='%s'>\n", sys->AdmPrgName);
  printf("    <td align=left><font size=2><input type=hidden name='mode' value='config'><input type=hidden name='password' value='%s'><input type=button value='GLOBAL CONFIGURATION' onClick=\"forkPost('config', this.form)\"></font></td>\n", db->password);
  printf("    </form>\n");

  printf("  </tr>\n");
  printf("</table>\n");
  tailHtml();
}

ADMList *getList(SysConfig *sys, int *tcount) {
  char    **dblist, *tmp;
  ADMList *info;
  DIR     *dp;
  struct  dirent *dirp;      /* GCC일경우 활성화 */
/*  struct  direct *dirp; */ /* CC일경우 활성화 */
  int     i, j, count;
  char    dbPath[0xff], dbDir[0xff], statDir[0xff];
  char    name[MAX_DBNAME_LEN + 1];

  snprintf(dbPath, sizeof(dbPath), "%s/%s", sys->RealBasePath, sys->DataDirectory);

  /* DB의 갯수를 파악 한다 */
  if((dp = opendir(dbPath)) == NULL) qError("Database directory(%s) not found.", dbPath);
  for(count = 0; (dirp = readdir(dp)) != NULL; ) {
    if(getDBname(name, sizeof(name), dirp->d_name, sys->DataExtension) == TRUE) count++;
  }
  closedir(dp);

  *tcount = count;
  if( count == 0 ) return NULL; /* db 없음 */

  /* db를 리스트에 저장 */
  dblist = (char **)malloc(sizeof(char *) * (count + 1));  /* 0번 미사용 */

  if(dblist == NULL) qError("Memory allocation fail.");

  if((dp = opendir(dbPath)) == NULL) qError("Database directory(%s) not found.", dbPath);
  for(i = 0; (dirp = readdir(dp)) != NULL; ) {
    if(getDBname(name, sizeof(name), dirp->d_name, sys->DataExtension) == FALSE) continue;
    if(++i > count) qError("Fatal error. Count mismatch.");

    if((tmp = strdup(name)) == NULL) qError("Memory allocation fail.");
    dblist[i] = tmp;
  }
  closedir(dp);
  if(i != count) qError("Fatal error. Count mismatch.");

  /* list sort */
  for(i = 1; i < count; i++) {
    for(j = i; j <=count; j++) {
      if(strcmp(dblist[i], dblist[j]) > 0) {  /* sweep */
        tmp = dblist[i];
        dblist[i] = dblist[j];
        dblist[j] = tmp;
      }
    }
  }

  /* Fetch Info */
  info = (ADMList *)malloc(sizeof(ADMList) * (count + 1));  /* 0번 미사용 */
  if( info == NULL ) qError("Memory allocation fail.");

  for(i = 1; i <= count; i++) {
    /* clear */
    strcpy(info[i].DBname,   "");
    info[i].Type          = NULL;
    info[i].TotalArticles = 0;
    info[i].TodayArticles = 0;
    info[i].UploadSize    = 0;
    info[i].TotalHits     = 0;
    info[i].HitsDay       = 0;
    info[i].TodayHits     = 0;
    info[i].Password      = NULL;

    /* Store DB Name */
    snprintf(info[i].DBname, sizeof(info[i].DBname), "%s", dblist[i]);

    /* make db, log : system path */
    snprintf(dbDir,   sizeof(dbDir),   "%s/%s/%s.%s", sys->RealBasePath, sys->DataDirectory, info[i].DBname, sys->DataExtension);
    snprintf(statDir, sizeof(statDir), "%s/%s/%s.%s", sys->RealBasePath, sys->StatDirectory, info[i].DBname, sys->StatExtension);

    fetchDBinfo(dbDir, statDir, &info[i]);
  }
 return info;
}

/* "NAME.DataExtension" -> NAME; NAME must be 0-9, A-Z, a-z (it is put into HTML and JavaScript) */
int getDBname(char *dbname, int size, char *filename, char *ext) {
  int len, namelen;

  len     = strlen(filename);
  namelen = len - (int)strlen(ext) - 1;
  if(namelen <= 0 || namelen > MAX_DBNAME_LEN || namelen >= size) return FALSE;
  if(filename[namelen] != '.' || strcmp(filename + namelen + 1, ext)) return FALSE;

  strncpy(dbname, filename, namelen);
  dbname[namelen] = '\0';
  if(qStr09AZaz(dbname) == FALSE) return FALSE;

  return TRUE;
}

void fetchDBinfo(char *dbDir, char *statDir, ADMList *info) {
  struct tm *localtime;
  char dateStr[8 + 1], runDate[8 + 1];
  int  i, lastNum;
  int  year, mon, day, daycount, stats;
  char *tmp;

  localtime = qGetTime();
  snprintf(dateStr, sizeof(dateStr), "%04d%02d%02d", localtime->tm_year, localtime->tm_mon, localtime->tm_mday);

  DBOpen(dbDir, GDBM_READER);
  lastNum = DBiFetch("conf.LastNum");

  if((info->Password = DBFetch("conf.Password")) == NULL) info->Password = "";

  info->Type = "public";
  if((tmp = DBFetch("conf.ListMode"))  != NULL) { if(strcmp(tmp, "anybody")) info->Type = "<font color='red'>secure</font>"; free(tmp); }
  if((tmp = DBFetch("conf.ReadMode"))  != NULL) { if(strcmp(tmp, "anybody")) info->Type = "<font color='red'>secure</font>"; free(tmp); }
  if((tmp = DBFetch("conf.WriteMode")) != NULL) { if(strcmp(tmp, "anybody")) info->Type = "<font color='red'>secure</font>"; free(tmp); }

  else info->Type = "<font color=red>empty</font>";

  /* Total Articles, Today Articles, UploadSize */
  for(i = 1; i <= lastNum; i++) {
    if((tmp = DBFetch("%d.Date", i)) == NULL) continue;
    info->TotalArticles++;
    if(!strncmp(tmp, dateStr, strlen(dateStr))) info->TodayArticles++;
    info->UploadSize += DBiFetch("%d.Filesize", i);
    free(tmp);
  }

  DBClose();

  /* 로그 분석 */
  info->TotalHits = 0;
  info->HitsDay   = 0;
  info->TodayHits = 0;


  if(qCheckFile(statDir) == FALSE) strcpy(info->Started, "<font color=red>unknown</font>");
  else {
     DBOpen(statDir, GDBM_READER);

    /* 로그 시작일 해석 */
    if((tmp = DBFetch("stat.Started")) == NULL) tmp = strdup("19000000000000");
    if(tmp == NULL || sscanf(tmp, "%04d%02d%02d", &year, &mon, &day) != 3 || year < 1900 || year > 9999 || mon < 0 || mon > 12 || day < 0 || day > 31) {
      year = 1900, mon = 0, day = 0;
    }
    free(tmp);

    snprintf(info->Started, sizeof(info->Started), "%02d%02d%02d", year-1900, mon, day);

    for(daycount = 0; ; day++) {
      if(day == 32) mon++, day = 1;
      if(mon == 13) year++, mon = 1;

      snprintf(runDate, sizeof(runDate), "%04d%02d%02d", year, mon, day);
      if(strcmp(runDate, dateStr) > 0) break;

      stats = DBiFetch("stat.%s", runDate);
      info->TotalHits  += stats;

      /* 금일일 경우 */
      if(!strcmp(runDate, dateStr)) info->TodayHits += stats;

      if(stats != 0) daycount++;
    }
    if(daycount == 0) info->HitsDay = 0;
    else info->HitsDay = info->TotalHits / daycount;

    DBClose();
  }
}
