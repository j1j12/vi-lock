#!/bin/sh
# Invoke with sh /path/install_board_tls.sh /path/to/board
set -eu
umask 077
[ "$(id -u)" = 0 ] || { echo 'Run as root' >&2; exit 1; }
[ "$#" = 1 ] || { echo 'Provide source board certificate directory' >&2; exit 1; }
src=$1
dest=/home/root/access-control-tls
[ ! -e "$dest" ] && [ ! -L "$dest" ] || { echo 'Destination exists; refusing overwrite' >&2; exit 1; }
for name in ca.pem client.pem client-key.pem; do
    [ -f "$src/$name" ] && [ ! -L "$src/$name" ] || { echo "Missing regular file: $name" >&2; exit 1; }
done
mkdir -m 700 "$dest"
for name in ca.pem client.pem client-key.pem; do
    cp "$src/$name" "$dest/$name"
    chmod 600 "$dest/$name"
done
sync
echo 'PASS: board credentials installed. No service or door-control changes.'
