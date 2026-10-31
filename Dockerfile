# syntax=docker/dockerfile:1

##############################################################################
## Stage 1 : build static, hardened binaries
##############################################################################
FROM debian:trixie-slim AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends gcc make libc6-dev libcrypt-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY src/ ./

# 1998 code : gnu89 keeps implicit declarations as warnings with modern gcc.
# Static PIE keeps ASLR, the runtime image needs no extra libraries.
ARG HARDEN="-O2 -fPIE -fstack-protector-strong -fstack-clash-protection -D_FORTIFY_SOURCE=2"
ARG LDHARDEN="-static-pie -Wl,-z,relro,-z,now,-z,noexecstack"

RUN set -eux; \
    CC="gcc -std=gnu89"; \
    CF="$HARDEN -w"; \
    (cd gdbm-1.7.3 && ./configure CC="$CC" >/dev/null \
      && make libgdbm.a CC="$CC" CFLAGS="$CF -DHAVE_RENAME=1" AR=ar ARFLAGS=rc RANLIB=ranlib); \
    for d in qDecoder-4.3.1 md5; do \
      make -C $d clean all CC="$CC" CFLAGS="$CF" AR=ar ARFLAGS=rc RANLIB=ranlib; \
    done; \
    for d in cwb-5.0 adm-1.2; do \
      make -C $d clean all CC="$CC" CFLAGS="$CF" EXTLIB="-lcrypt $LDHARDEN"; \
    done; \
    $CC $CF -o md5genkey-1.0/md5genkey md5genkey-1.0/md5genkey.c md5/libmd5.a $LDHARDEN; \
    for b in cwb-5.0/CrazyWWWBoard.cgi adm-1.2/CrazyAdmin.cgi md5genkey-1.0/md5genkey; do \
      if ldd "$b" 2>&1 | grep -q '=>'; then echo "$b is not static" >&2; exit 1; fi; \
    done


##############################################################################
## Stage 2 : nginx (non-root) + fcgiwrap running the CGIs
##############################################################################
FROM nginxinc/nginx-unprivileged:1.30-trixie

USER root

# fcgiwrap runs CGIs for nginx. No setuid/setgid binary is left in the image.
RUN apt-get update \
 && apt-get install -y --no-install-recommends fcgiwrap \
 && rm -rf /var/lib/apt/lists/* /etc/nginx/conf.d/* /docker-entrypoint.d \
 && find / -xdev -type f -perm /6000 -exec chmod a-s {} +

# Binaries and static files belong to root, the service user can only read them.
COPY --from=build --chown=root:root --chmod=0755 \
     /src/cwb-5.0/CrazyWWWBoard.cgi /src/adm-1.2/CrazyAdmin.cgi /usr/lib/cgi-bin/
COPY --from=build --chown=root:root --chmod=0755 \
     /src/md5genkey-1.0/md5genkey /usr/local/libexec/cwb/md5genkey
COPY --chown=root:root conf/CrazyWWWBoard.conf-dist_en conf/CrazyWWWBoard.conf-dist_kr conf/CrazyWWWBoard.conf-dist_jp /etc/cwb/
COPY --chown=root:root cwb-data/icon      /srv/cwb-data/icon
COPY --chown=root:root cwb-data/icon-stat /srv/cwb-data/icon-stat
COPY --chown=root:root cwb-data/icon-type /srv/cwb-data/icon-type
COPY --chown=root:root cwb-data/message   /srv/cwb-data/message
COPY --chown=root:root docker/nginx.conf docker/cwb_fastcgi.conf /etc/nginx/
COPY --chown=root:root --chmod=0755 docker/entrypoint.sh /usr/local/bin/cwb-entrypoint

# CGIs read their config from the working directory (cgi-bin). The real files
# are generated at start into /run/cwb, so cgi-bin stays read-only.
# /srv/cwb-data/var is the only persistent writable place (boards, uploads, stat, log).
RUN ln -s /run/cwb/CrazyWWWBoard.conf /usr/lib/cgi-bin/CrazyWWWBoard.conf \
 && ln -s /run/cwb/CrazyAdmin.conf    /usr/lib/cgi-bin/CrazyAdmin.conf \
 && install -d -o nginx -g nginx -m 0700 /run/cwb \
 && install -d -o nginx -g nginx -m 0750 /srv/cwb-data/var \
 && chmod -R go-w /srv/cwb-data/icon /srv/cwb-data/icon-stat /srv/cwb-data/icon-type /srv/cwb-data/message /etc/cwb

USER 101:101
EXPOSE 8080
VOLUME ["/srv/cwb-data/var"]
STOPSIGNAL SIGTERM
ENTRYPOINT ["/usr/local/bin/cwb-entrypoint"]
