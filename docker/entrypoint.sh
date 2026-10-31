#!/bin/bash
# CrazyWWWBoard container entrypoint
#  - generates runtime configs into /run/cwb (tmpfs), the root filesystem stays read-only
#  - starts fcgiwrap and nginx, stops the container when one of them exits
set -euo pipefail

RUN=/run/cwb
BASE=/srv/cwb-data
VAR=$BASE/var

die() { echo "cwb-entrypoint: $*" >&2; exit 1; }

: "${CWB_SERVER_NAME:=localhost}"
: "${CWB_SERVER_PORT:=8080}"
: "${CWB_ADMIN_NAME:=Administrator}"
: "${CWB_ADMIN_EMAIL:=admin@example.com}"
: "${CWB_LANG:=en}"
: "${CWB_ADMIN_ALLOW:=all}"
: "${CWB_FCGI_CHILDREN:=4}"

# Values go into config files, accept only plain characters
[[ $CWB_SERVER_NAME =~ ^[A-Za-z0-9.-]{1,253}$ ]]       || die "invalid CWB_SERVER_NAME"
[[ $CWB_SERVER_PORT =~ ^[0-9]{1,5}$ ]]                 || die "invalid CWB_SERVER_PORT"
[[ $CWB_ADMIN_NAME =~ ^[A-Za-z0-9\ ._-]{1,30}$ ]]      || die "invalid CWB_ADMIN_NAME"
[[ $CWB_ADMIN_EMAIL =~ ^[A-Za-z0-9._-]+@[A-Za-z0-9.-]+$ ]] || die "invalid CWB_ADMIN_EMAIL"
[[ $CWB_LANG =~ ^(en|kr|jp)$ ]]                        || die "CWB_LANG must be en, kr or jp"
[[ $CWB_FCGI_CHILDREN =~ ^[0-9]{1,2}$ ]]               || die "invalid CWB_FCGI_CHILDREN"

[ -w "$RUN" ] || die "$RUN must be writable (mount tmpfs there)"
[ -w "$VAR" ] || die "$VAR must be writable by uid $(id -u) (data volume)"
umask 077

# Persistent data
mkdir -p "$VAR/data" "$VAR/stat" "$VAR/log"

# Board passwords are crypt()ed with this salt, so it is generated once and kept
if [ ! -s "$VAR/.crypt_salt" ]; then
  head -c 256 /dev/urandom | LC_ALL=C tr -dc 'A-Za-z0-9./' | cut -c1-2 > "$VAR/.crypt_salt"
fi
salt=$(cat "$VAR/.crypt_salt")
[[ $salt =~ ^[A-Za-z0-9./]{2}$ ]] || die "broken $VAR/.crypt_salt"

# License key is md5 of the URL of cgi-bin
license=$(printf 'CWB98PE2|http://%s:%s/cgi-bin/\n' "$CWB_SERVER_NAME" "$CWB_SERVER_PORT" \
          | /usr/local/libexec/cwb/md5genkey | sed -n 's/^\* MD5 result *: \[\(.*\)\]$/\1/p')
[ -n "$license" ] || die "license key generation failed"

# CrazyWWWBoard.conf
sed -e "s%@LicenseKey@%$license%" \
    -e "s%@AdminName@%$CWB_ADMIN_NAME%" \
    -e "s%@AdminEmail@%$CWB_ADMIN_EMAIL%" \
    -e "s%@RealBasePath@%$BASE%" \
    -e "s%@HtmlBasePath@%/cwb-data%" \
    -e "s%@Help@%message/help-$CWB_LANG.txt%" \
    -e "s%@Sendmail@%/bin/cat%" \
    -e "s%^SendmailOptions *=.*%SendmailOptions = > /dev/null%" \
    -e "s%^DataDirectory *=.*%DataDirectory = var/data%" \
    -e "s%^StatDirectory *=.*%StatDirectory = var/stat%" \
    -e "s%^LogDirectory *=.*%LogDirectory = var/log%" \
    -e "s%^CryptSalt *=.*%CryptSalt = $salt%" \
    "/etc/cwb/CrazyWWWBoard.conf-dist_$CWB_LANG" > "$RUN/CrazyWWWBoard.conf.tmp"
chmod 0400 "$RUN/CrazyWWWBoard.conf.tmp"
mv -f "$RUN/CrazyWWWBoard.conf.tmp" "$RUN/CrazyWWWBoard.conf"

# Master password of CrazyAdmin.cgi : no default, CrazyAdmin.cgi fails closed without it
password=""
if [ -n "${CWB_ADMIN_PASSWORD_FILE:-}" ]; then
  [ -r "$CWB_ADMIN_PASSWORD_FILE" ] || die "can not read CWB_ADMIN_PASSWORD_FILE"
  password=$(head -n 1 "$CWB_ADMIN_PASSWORD_FILE" | tr -d '\r\n')
elif [ -n "${CWB_ADMIN_PASSWORD:-}" ]; then
  password=$CWB_ADMIN_PASSWORD
fi
unset CWB_ADMIN_PASSWORD
rm -f "$RUN/CrazyAdmin.conf"
if [ -n "$password" ]; then
  [[ $password =~ ^[0-9A-Za-z]{2,8}$ ]] || die "admin password must be 2-8 characters of 0-9 A-Z a-z"
  CWB_PASS=$password perl -e 'print crypt($ENV{CWB_PASS}, $ARGV[0]), "\n"' "$salt" > "$RUN/CrazyAdmin.conf"
  chmod 0400 "$RUN/CrazyAdmin.conf"
else
  echo "cwb-entrypoint: no admin password given, CrazyAdmin.cgi is locked" >&2
fi
unset password

# nginx runtime snippets
{
  printf 'fastcgi_param SERVER_NAME "%s";\n' "$CWB_SERVER_NAME"
  printf 'fastcgi_param SERVER_PORT "%s";\n' "$CWB_SERVER_PORT"
} > "$RUN/server_name.conf"

{
  for net in $CWB_ADMIN_ALLOW; do
    if [ "$net" = "all" ]; then echo "allow all;"; continue; fi
    [[ $net =~ ^[0-9A-Fa-f.:/]+$ ]] || die "invalid CWB_ADMIN_ALLOW entry: $net"
    echo "allow $net;"
  done
  echo "deny all;"
} > "$RUN/admin_access.conf"

nginx -t -q

# Run both, stop everything when one of them dies
fcgiwrap -c "$CWB_FCGI_CHILDREN" -s "unix:$RUN/fcgiwrap.sock" &
fcgi_pid=$!
for _ in $(seq 50); do [ -S "$RUN/fcgiwrap.sock" ] && break; sleep 0.1; done
[ -S "$RUN/fcgiwrap.sock" ] || die "fcgiwrap did not start"

nginx -g 'daemon off;' &
nginx_pid=$!

status=1
trap 'status=0; kill -TERM $nginx_pid $fcgi_pid 2>/dev/null' TERM INT
wait -n $nginx_pid $fcgi_pid || true
kill -TERM $nginx_pid $fcgi_pid 2>/dev/null || true
wait || true
exit $status
