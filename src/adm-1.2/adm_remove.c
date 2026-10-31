#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "gdbm.h"
#include "gdbm_lib.h"
#include "qDecoder.h"
#include "adm.h"
#include "adm_remove.h"

static void checkDBname(DB *db);
static void rmUploadDir(char *path);

void adm_rm_log(SysConfig *sys, DB *db) {
  char access_log[0xff], error_log[0xff];

  checkDBname(db);
  if(snprintf(access_log, sizeof(access_log), "%s/%s/access_log.%s", sys->RealBasePath, sys->LogDirectory, db->db) >= (int)sizeof(access_log)) qError("Path is too long.");
  if(snprintf(error_log,  sizeof(error_log),  "%s/%s/error_log.%s",  sys->RealBasePath, sys->LogDirectory, db->db) >= (int)sizeof(error_log)) qError("Path is too long.");

  if(qCheckFile(access_log) == TRUE) {
    if(unlink(access_log) != 0) qError("Access-log(%s) delete fail.", access_log);
  }
  if(qCheckFile(error_log) == TRUE) {
    if(unlink(error_log)  != 0) qError("Error-log(%s) delete fail.", error_log);
  }
}

void adm_rm_stat(SysConfig *sys, DB *db) {
  char statpath[0xff];

  checkDBname(db);
  if(snprintf(statpath, sizeof(statpath), "%s/%s/%s.%s", sys->RealBasePath, sys->StatDirectory, db->db, sys->StatExtension) >= (int)sizeof(statpath)) qError("Path is too long.");

  if(qCheckFile(statpath) == TRUE) {
    if(unlink(statpath) != 0) qError("Statistics(%s) delete fail.", statpath);
  }

}

void adm_rm_db(SysConfig *sys, DB *db) {
  char dbpath[0xff], buf[0xff];

  checkDBname(db);
  if(snprintf(dbpath, sizeof(dbpath), "%s/%s/%s.%s", sys->RealBasePath, sys->DataDirectory, db->db, sys->DataExtension) >= (int)sizeof(dbpath)) qError("Path is too long.");

  /* 로그 삭제 */
  adm_rm_log(sys, db);

  /* 통계 삭제 */
  adm_rm_stat(sys, db);

  /* db 삭제 */
  if(qCheckFile(dbpath) == TRUE) {
    if(unlink(dbpath) != 0) qError("Database(%s) delete fail.", dbpath);
  }
  else qError("There is no database named %s(%s).", db->db, dbpath);

  /* 업로드 디렉토리 삭제 */
  if(snprintf(buf, sizeof(buf), "%s/%s/%s", sys->RealBasePath, sys->DataDirectory, db->db) >= (int)sizeof(buf)) qError("Path is too long.");
  rmUploadDir(buf);
}

/* db name is a part of file paths : never empty, only 0-9, A-Z, a-z */
static void checkDBname(DB *db) {
  if(!strcmp(db->db, "") || qStr09AZaz(db->db) == FALSE || strlen(db->db) > MAX_DBNAME_LEN) {
    qError("Invalid database name. Do not use manually.");
  }
}

/* remove the flat upload directory : regular files only, no recursion */
static void rmUploadDir(char *path) {
  DIR    *dp;
  struct dirent *dirp;
  struct stat st;
  char   filepath[0xff];

  /* no upload directory, or not a real directory (symlink) */
  if(lstat(path, &st) != 0 || !S_ISDIR(st.st_mode)) return;
  if((dp = opendir(path)) == NULL) return;
  while((dirp = readdir(dp)) != NULL) {
    if(!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, "..")) continue;
    if(snprintf(filepath, sizeof(filepath), "%s/%s", path, dirp->d_name) >= (int)sizeof(filepath)) continue;
    if(lstat(filepath, &st) != 0 || S_ISDIR(st.st_mode)) continue;
    if(unlink(filepath) != 0) qError("Upload file(%s) delete fail.", filepath);
  }
  closedir(dp);

  if(rmdir(path) != 0) qError("Upload directory(%s) delete fail.", path);
}
