#!/bin/sh
# Runs the example against the server in $CONNINFO and checks the export.
set -eu
export LC_ALL=C
molto=$1
cd inventory
"$molto" run -- "$CONNINFO" > inventory.xml
cat inventory.xml
grep -q 'encoding="ISO-8859-1"' inventory.xml
grep -q 'quantity="12"' inventory.xml
# "café en grano" in ISO-8859-1: é is the single byte 0xE9.
grep -q "$(printf 'caf\351 en grano')" inventory.xml
