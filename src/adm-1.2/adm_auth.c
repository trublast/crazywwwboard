#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "gdbm.h"
#include "gdbm_lib.h"
#include "qDecoder.h"
#include "adm.h"
#include "adm_auth.h"

/* user authentication gateway. 비밀번호가 상이할경우 비밀번호 입력 폼을 출력한다 */
void adm_auth(SysConfig *sys, DB *db){
  FILE *fp;
  char sysPass[0xff];

  strcpy(sysPass, "");

  /* password 획득 */
  if((fp = fopen(PASSWORD_FILE, "rt")) != NULL) {
    if(fgets(sysPass, sizeof(sysPass), fp) == NULL) strcpy(sysPass, "");
    fclose(fp);
  }

  /* fail closed : no default password, the administrator must create the file */
  qRemoveSpace(sysPass);
  if(strlen(sysPass) == 0) adm_nopassfile(sys);

  /* 비밀번호가 없다면, 확인화면 출력 */
  if(!strcmp(db->password, "")) adm_authform(sys, db);

  /* feedback되었다, System password와 비교한다 */
  if(!strcmp(sysPass, db->password)) return;

  /* 비밀번호 재 확인 */
  adm_authform(sys, db);  
}

void adm_authform(SysConfig *sys, DB *db){
  headHtml(sys);

  printf("<table border=0>\n");

  printf("<form method='post' action='%s'>\n", sys->AdmPrgName);
  printf("  <input type='hidden' name='db'         value='%s'>\n", qHtmlEncode(db->db));
  printf("  <input type='hidden' name='mode'       value='%s'>\n", qHtmlEncode(db->mode));

  if(license_valid == TRUE) {
    printf("  <tr><td colspan=2 align=center><font size=3 face='arial' color=blue><b>\n");
    printf("    LICENSE KEY IS VERIFIED\n");
    printf("  </b></font><br><br></td></tr>\n");
  }

  printf("  <tr>\n");
  printf("    <td align=right><font size=2 face='arial'><b>P a s s w o r d</b></font></td>\n");
  printf("    <td align=left><font size=3><input type='password' name='password' size=20 maxlength=%d></font></td>\n", MAX_PASSWORD_LEN);
  printf("  </tr>\n");

  printf("  <tr><td colspan=2><br></td></tr>\n");

  printf("  <tr>\n");
  printf("    <td align=center colspan=2><font size=2><input type='submit' value='        E N T E R        '></font></td>\n");
  printf("  </tr>\n");

  printf("  <tr><td colspan=2><br></td></tr>\n");

  printf("</form>\n");
  printf("</table>\n");

  tailHtml();

  exit(0);
}

void adm_authsave(SysConfig *sys, DB *db){
}

/* master password file is missing or empty : explain how to create it */
void adm_nopassfile(SysConfig *sys){
  headHtml(sys);

  printf("<table border=0 width='%s'><tr><td><font size=2 face='arial'>\n", LIST_WIDTH);
  printf("<font color=red><b>Master password file '%s' is missing or empty.</b></font><br><br>\n", PASSWORD_FILE);
  printf("For security reasons %s does not create a default password.\n", OFFICIAL_ADMPRG_NAME);
  printf("The administrator must create the file '%s' in the directory of %s\n", PASSWORD_FILE, OFFICIAL_ADMCGI_NAME);
  printf("(the same directory as %s) by hand.<br><br>\n", REALTIME_CONFIG_FILE);
  printf("The file must contain a single line : the value of <b>crypt(password, CryptSalt)</b>,\n");
  printf("where CryptSalt is the value of the 'CryptSalt' entry in %s\n", REALTIME_CONFIG_FILE);
  printf("and the password is 2 to %d characters of 0-9, A-Z, a-z.\n", MAX_PASSWORD_LEN);
  printf("Make the file readable only by the owner of %s (chmod 600).<br><br>\n", OFFICIAL_ADMCGI_NAME);
  printf("Example : <tt>perl -e 'print crypt(\"PASSWORD\", \"SALT\"), \"\\n\"' &gt; %s</tt>\n", PASSWORD_FILE);
  printf("</font></td></tr></table>\n");

  tailHtml();

  exit(0);
}
