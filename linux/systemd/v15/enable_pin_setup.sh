#!/bin/sh
# Explicit root authorization for first setup; no default PIN and no auto reset.
set -eu
[ "$(id -u)" = 0 ] || { echo 'Root required' >&2; exit 1; }
directory=/home/root/access-control-data
[ ! -L "$directory" ] || { echo 'Refusing symlink directory' >&2; exit 1; }
mkdir -p "$directory"
chmod 700 "$directory"
for item in admin-pin.json admin-pin.setup; do
    [ ! -e "$directory/$item" ] && [ ! -L "$directory/$item" ] || {
        echo "STOP: $item already exists; no reset performed" >&2; exit 1;
    }
done
umask 077
set -C
: > "$directory/admin-pin.setup"
echo 'One-time first setup is enabled. Open Personnel Management now and choose your own 8-12 digit PIN.'
echo 'No PIN was generated or printed. Do not leave the board unattended before completing setup.'
