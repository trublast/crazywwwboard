# CrazyWWWBoard in Docker

Two-stage image:

1. `build` (debian:trixie-slim): builds static PIE binaries with
   `-fstack-protector-strong -fstack-clash-protection -D_FORTIFY_SOURCE=2`
   and full RELRO.
2. runtime (`nginxinc/nginx-unprivileged:1.30-trixie`): nginx serves static
   files and runs `CrazyWWWBoard.cgi` / `CrazyAdmin.cgi` through fcgiwrap.
   `CrazyArchive` (mail delivery agent) is not included.

## Run

```sh
echo 'MyPass12' > admin_password.txt && chmod 600 admin_password.txt
sudo mkdir -p /var/lib/crazywwwboard      # or set CWB_DATA_DIR
docker compose up -d --build
```

Open `http://localhost:8080/cgi-bin/CrazyAdmin.cgi`, create a board, then
open `http://localhost:8080/cgi-bin/CrazyWWWBoard.cgi?db=NAME` and configure
it on the first visit.

## Settings (environment)

| Variable | Default | Meaning |
|---|---|---|
| `CWB_SERVER_NAME` | `localhost` | host name users see (license key, links in pages and mails) |
| `CWB_SERVER_PORT` | `8080` | port users see |
| `CWB_ADMIN_NAME`, `CWB_ADMIN_EMAIL` | | system administrator |
| `CWB_LANG` | `en` | `en`, `kr`, `jp` |
| `CWB_ADMIN_PASSWORD_FILE` / `CWB_ADMIN_PASSWORD` | none | master password of CrazyAdmin.cgi, 2-8 chars `[0-9A-Za-z]`. Without it CrazyAdmin.cgi is locked |
| `CWB_ADMIN_ALLOW` | `all` | CIDRs allowed to open CrazyAdmin.cgi |
| `CWB_FCGI_CHILDREN` | `4` | fcgiwrap workers |
| `CWB_DATA_DIR` (compose) | `/var/lib/crazywwwboard` | data directory on the docker host |

## Hardening

- Runs as uid 101, all capabilities dropped, `no-new-privileges`, no
  setuid/setgid file in the image, pids and memory limits.
- Read-only root filesystem. Writable mounts, all `noexec,nosuid,nodev`:
  - `/tmp` (tmpfs) : nginx temporary files
  - `/run/cwb` (tmpfs) : generated configs, fcgiwrap socket
  - `/srv/cwb-data/var` : boards, uploads, statistics, logs (persistent)
- Docker ignores `noexec` for bind mounts, so compose mounts the data
  directory as an overlay volume (`CWB_DATA_DIR/upper` keeps the data).
  `cwb-init` creates the directories before the board starts.
  The volume options are fixed when the volume is created: after changing
  `CWB_DATA_DIR` run `docker compose down` and `docker volume rm <project>_cwb-var`.
  Overlay needs a Linux host with the data dir on ext4/xfs/btrfs. Docker
  Desktop can't see host paths there; use a plain named volume instead
  (without noexec).
- Only the two CGIs can be executed, `SERVER_NAME` / `SERVER_PORT` are fixed
  (not taken from the `Host` header).
- Uploads are served as passive content: images, text or
  `application/octet-stream`, with `Content-Security-Policy: sandbox` and
  `nosniff`. Database files, statistics and logs are not served.
- The crypt salt is random per installation (`var/.crypt_salt`).

## Limitations

- Mail is discarded (no MTA in the image, `Sendmail = /bin/cat > /dev/null`).
- The master password can't be changed from the web UI and
  `CrazyWWWBoard.conf` changes made in CrazyAdmin.cgi are lost on restart:
  the configuration is generated at start from the environment.
- Use a TLS terminating proxy in front of the container.
