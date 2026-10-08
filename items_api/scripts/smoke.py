#!/usr/bin/env python3
"""Exercise the live API while the server and PostgreSQL are running."""
import datetime
import json
import sys
import urllib.error
import urllib.request

base = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8080"

def request(method, path, body=None, expected=200, raw=None, headers=None):
    data = json.dumps(body).encode() if body is not None else raw
    options = {"Content-Type": "application/json"} if data is not None else {}
    options.update(headers or {})
    req = urllib.request.Request(base + path, data=data, headers=options, method=method)
    try:
        response = urllib.request.urlopen(req, timeout=5)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        content = response.read()
        assert response.status == expected, (method, path, response.status, content)
        return json.loads(content) if content else None

item_id = None
try:
    request("GET", "/health")
    name = "O'Reilly \"keyboard\" Café; DROP TABLE items; --"
    item = request("POST", "/items", {"name": name, "price": 19.99, "stock": 4}, expected=201)
    item_id = item["id"]
    path = f"/items/{item_id}"
    assert item["name"] == name and item["price"] == 19.99 and item["stock"] == 4
    created = item["created_at"]
    updated = item["updated_at"]
    assert request("GET", path) == item
    page = request("GET", "/items?limit=2&offset=0")
    assert isinstance(page, list) and len(page) <= 2
    patch = request("PATCH", path, {"stock": 0})
    assert patch["name"] == name and patch["price"] == 19.99 and patch["stock"] == 0
    assert patch["created_at"] == created
    assert datetime.datetime.fromisoformat(patch["updated_at"]) > datetime.datetime.fromisoformat(updated)
    renamed = request("PATCH", path, {"name": "Mouse", "price": 7.50})
    assert renamed["name"] == "Mouse" and renamed["price"] == 7.50 and renamed["stock"] == 0
    request("PATCH", path, {"created_at": "now"}, expected=422)
    request("PATCH", path, {}, expected=422)
    request("POST", "/items", {"name": "Bad", "price": -1, "stock": 0}, expected=422)
    request("POST", "/items", expected=400, raw=b"[]")
    request("POST", "/items", expected=415, raw=b"{}", headers={"Content-Type": "text/plain"})
    request("POST", "/items", expected=413, raw=b" " * 4097)
    request("GET", "/items?limit=0", expected=400)
    request("GET", "/items/0", expected=400)
    request("GET", "/missing", expected=404)
    request("PUT", path, expected=405)
    request("DELETE", path, expected=204)
    request("GET", path, expected=404)
    request("DELETE", path, expected=404)
    item_id = None
    print("smoke: all CRUD and HTTP checks passed")
finally:
    if item_id is not None:
        request("DELETE", f"/items/{item_id}", expected=204)
