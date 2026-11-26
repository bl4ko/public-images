#!/bin/bash
set -euo pipefail

fail() {
  echo "openso-entrypoint: $*" >&2
  exit 1
}

for v in OPENSO_DB_HOST OPENSO_DB_NAME OPENSO_DB_USER OPENSO_DB_PASSWORD OPENSO_SECRET OPENSO_PUBLIC_HOST OPENSO_API_URL; do
  [ -n "${!v:-}" ] || fail "$v is required"
done
[[ $OPENSO_SECRET =~ ^[0-9a-fA-F]{64}$ ]] || fail "OPENSO_SECRET must be 64 hex characters"
[[ $OPENSO_PUBLIC_HOST =~ ^[A-Za-z0-9.-]+$ ]] || fail "OPENSO_PUBLIC_HOST must be a bare hostname or IPv4 address"
[[ $OPENSO_API_URL =~ ^https?://[^[:space:]]+$ ]] || fail "OPENSO_API_URL must be an http(s) URL"
OPENSO_DB_PORT=${OPENSO_DB_PORT:-3306}
[[ $OPENSO_DB_PORT =~ ^[0-9]+$ ]] || fail "OPENSO_DB_PORT must be a number"
OPENSO_GAME_LOCATION=${OPENSO_GAME_LOCATION:-/game/TSOClient/}
OPENSO_NFS=${OPENSO_NFS:-/nfs}
OPENSO_USE_PROXY=${OPENSO_USE_PROXY:-false}
[[ $OPENSO_USE_PROXY =~ ^(true|false)$ ]] || fail "OPENSO_USE_PROXY must be true or false"

umask 077
jq \
  --arg game "$OPENSO_GAME_LOCATION" \
  --arg nfs "$OPENSO_NFS" \
  --arg secret "$OPENSO_SECRET" \
  --arg host "$OPENSO_DB_HOST" \
  --arg port "$OPENSO_DB_PORT" \
  --arg db "$OPENSO_DB_NAME" \
  --arg user "$OPENSO_DB_USER" \
  --arg pw "$OPENSO_DB_PASSWORD" \
  --arg public "$OPENSO_PUBLIC_HOST" \
  --arg api "${OPENSO_API_URL%/}" \
  --argjson proxy "$OPENSO_USE_PROXY" \
  'def q: "\"" + gsub("\""; "\"\"") + "\"";
   .gameLocation = $game
   | .simNFS = $nfs
   | .secret = $secret
   | .database.connectionString = "server=\($host | q);port=\($port);database=\($db | q);uid=\($user | q);pwd=\($pw | q);"
   | .services.userApi.cdnUrl = $api
   | .services.userApi.useProxy = $proxy
   | .services.tasks.public_host = "\($public):35101"
   | .services.cities[0].public_host = "\($public):33101"
   | .services.lots[0].public_host = "\($public):34101"' \
  /etc/openso/config.template.json > /app/config.json.tmp
mv /app/config.json.tmp /app/config.json
umask 022

cd /app
if [ "$#" -gt 0 ]; then
  exec ./FSO.Server.Core "$@"
fi

[ -s "${OPENSO_GAME_LOCATION%/}/tuning.dat" ] || fail "${OPENSO_GAME_LOCATION%/}/tuning.dat is missing; run openso-fetch-tso first"
printf 'y\n' | ./FSO.Server.Core db-init
exec ./FSO.Server.Core run
