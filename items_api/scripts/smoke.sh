#!/usr/bin/env bash
# Exercise the live API while the server and PostgreSQL are running.
set -euo pipefail
command -v curl >/dev/null || { echo 'curl is required' >&2; exit 1; }
command -v jq >/dev/null || { echo 'jq is required' >&2; exit 1; }
base=${1:-http://127.0.0.1:8080}
base=${base%/}
item_id=
tmp=$(mktemp -d)
cleanup() {
    if [[ -n "$item_id" ]]; then
        curl --silent --max-time 5 -X DELETE "$base/items/$item_id" >/dev/null || true
    fi
    rm -rf "$tmp"
}
trap cleanup EXIT
request() {
    local method=$1 path=$2 expected=$3 status
    local -a args=()
    if [[ $# -ge 4 ]]; then
        args=(-H "Content-Type: ${5:-application/json}" --data-binary "$4")
    fi
    status=$(curl --silent --show-error --max-time 5 -X "$method" \
        -o "$tmp/body" -w '%{http_code}' ${args[@]+"${args[@]}"} "$base$path")
    if [[ "$status" != "$expected" ]]; then
        echo "$method $path: expected $expected, got $status" >&2
        cat "$tmp/body" >&2
        return 1
    fi
    response=$(cat "$tmp/body")
}
check() { jq -e "$@" <<< "$response" >/dev/null; }
request GET /health 200
name="O'Reilly \"keyboard\" Café; DROP TABLE items; --"
body=$(jq -nc --arg name "$name" '{name:$name,price:19.99,stock:4}')
request POST /items 201 "$body"
item_id=$(jq -er '.id | select(type == "number" and . > 0)' <<< "$response")
path=/items/$item_id
check --arg name "$name" '.name == $name and .price == 19.99 and .stock == 4'
item=$response
created=$(jq -r '.created_at' <<< "$response")
updated=$(jq -r '.updated_at' <<< "$response")
request GET "$path" 200
check --argjson item "$item" '. == $item'
request GET '/items?limit=2&offset=0' 200
check 'type == "array" and length <= 2'
request PATCH "$path" 200 '{"stock":0}'
check --arg name "$name" --arg created "$created" --arg updated "$updated" \
    '.name == $name and .price == 19.99 and .stock == 0 and .created_at == $created and .updated_at > $updated'
request PATCH "$path" 200 '{"name":"Mouse","price":7.50}'
check '.name == "Mouse" and .price == 7.5 and .stock == 0'
request PATCH "$path" 422 '{"created_at":"now"}'
request PATCH "$path" 422 '{}'
request POST /items 422 '{"name":"Bad","price":-1,"stock":0}'
request POST /items 400 '[]'
request POST /items 415 '{}' text/plain
oversized=$(printf '%4097s' '')
request POST /items 413 "$oversized"
request GET '/items?limit=0' 400
request GET /items/0 400
request GET /missing 404
request PUT "$path" 405
request DELETE "$path" 204
[[ -z "$response" ]]
request GET "$path" 404
request DELETE "$path" 404
item_id=
echo 'smoke: all CRUD and HTTP checks passed'
