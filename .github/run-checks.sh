#!/bin/sh
# Builds and runs each recipe's check/ with the molto given. libpq's also
# talks to the server in $CONNINFO.
set -eu
molto=$1
root=$(pwd)
for recipe in libiconv libxml2 libpq; do
    echo "== $recipe"
    cd "$root/.recipes/$recipe/check"
    LIBPQ_CHECK_CONNINFO="$CONNINFO" "$molto" run
done
