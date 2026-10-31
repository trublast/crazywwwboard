#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gdbm.h"
#include "gdbm_lib.h"
#include "qDecoder.h"
#include "adm.h"
#include "adm_create.h"

void adm_create(SysConfig *sys, DB *db) {
  char dbpath[0xff];

  if(!strcmp(db->db, "")) qError("The string for your new database is empty. Try again.");

  if(qStr09AZaz(db->db) == FALSE || strlen(db->db) > MAX_DBNAME_LEN) {
    qError("Only regular expression(0-9, A-Z, a-z) up to %d characters is allowed for the database name.", MAX_DBNAME_LEN);
  }

  if(snprintf(dbpath, sizeof(dbpath), "%s/%s/%s.%s", sys->RealBasePath, sys->DataDirectory, db->db, sys->DataExtension) >= (int)sizeof(dbpath)) qError("Path is too long.");

  if(qCheckFile(dbpath) == TRUE) qError("DB(%s) already exist. Use another one, please.", db->db);

  if(DBCreate(dbpath, 0644) != TRUE) qError("DB(%s) creation fail.", dbpath);
}
