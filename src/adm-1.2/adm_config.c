#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gdbm.h"
#include "gdbm_lib.h"
#include "qDecoder.h"
#include "adm.h"
#include "adm_message.h"
#include "adm_config.h"

/* file path related entries can not be changed through the web */
static char *protectedKeys[] = {
  "RealBasePath", "DataDirectory", "DataExtension", "StatDirectory", "StatExtension", "LogDirectory",
  "HtmlHead", "HtmlTail", "MailHead", "MailTail", "Sendmail", "SendmailOptions",
  NULL
};

void adm_config(SysConfig *sys, DB *db) {
  char *config;
  char *msg = "";

  config = readMessage(REALTIME_CONFIG_FILE);

/*
  if(license_valid == FALSE) msg = "onClick=\"alert('You can not store configurations under limited mode.');window.close();\"";
*/

  headHtml(sys);

  printf("<form method=post action='%s'>\n", sys->AdmPrgName);
  printf("<input type='hidden' name='mode'     value='configsave'>\n");
  printf("<input type='hidden' name='password' value='%s'>\n", db->password);

  printf("<table border=1 cellspacing=0 width=%s>\n", LIST_WIDTH);
  printf("  <tr><td bgcolor=#4c81ce align=center><font size=2 face='arial'><b>Global Configuration</b></font></td></tr>\n");
  printf("  <tr><td bgcolor=#f0f9ff align=center><textarea name='cConfig' cols=72 rows=20>"); qPuts(1, config); printf("</textarea></td></tr>\n");
  printf("</table>\n");

  printf("<br>\n");

  printf("<font size=2><input type='submit' value='              SAVE CONFIGURATION              ' %s></font>\n", msg);
  printf("</form>\n");

  tailHtml();
}

void adm_configsave(SysConfig *sys, DB *db) {
  Entry *oldconf, *newconf;
  char *config, *oldval, *newval, *p, *q;
  int  i;

  config = qValue("cConfig");

  if(config == NULL) qError("Do not use manually.");

  /* remove CR here, as saveMessage() does, so we check exactly what is stored */
  for(p = q = config; *p != '\0'; p++) if(*p != '\r') *q++ = *p;
  *q = '\0';

  /* protected entries must stay the same as in the current file */
  if((oldconf = qfDecoder(REALTIME_CONFIG_FILE)) == NULL) qError("Realtime configuration file(%s) not found.", REALTIME_CONFIG_FILE);
  newconf = qsDecoder(config);
  for(i = 0; protectedKeys[i] != NULL; i++) {
    oldval = qfValue(oldconf, protectedKeys[i]);
    newval = qsValue(newconf, protectedKeys[i]);
    if(oldval == NULL && newval == NULL) continue;
    if(oldval == NULL || newval == NULL || strcmp(oldval, newval)) {
      qError("'%s' can not be changed through the web. Edit %s on the server.", protectedKeys[i], REALTIME_CONFIG_FILE);
    }
  }
  qfFree(oldconf);
  qsFree(newconf);

  saveMessage(REALTIME_CONFIG_FILE, config);
}
