#!/usr/bin/env python3
"""Deterministic receiver protocol fixture, never touches the network."""
import json
import sys


def emit(event, **data):
    print(json.dumps(dict(event=event, **data)), flush=True)


item = None
position = 0
for line in sys.stdin:
    request = json.loads(line)
    op = request["op"]
    if op == "discover":
        emit("devices", devices=[dict(id="test-receiver", label="Synthetic receiver")])
    elif op == "connect":
        emit("connected")
    elif op == "play":
        item = request["id"]
        position = request["start"]
        emit("status", id=item, state="PAUSED" if request["paused"] else "PLAYING", position=position, duration=60)
    elif op == "pause":
        emit("status", id=item, state="PAUSED" if request["paused"] else "PLAYING", position=position, duration=60)
    elif op == "seek":
        position = request["position"]
        emit("status", id=item, state="PAUSED", position=position, duration=60)
    elif op == "stop":
        item = None
    elif op in ("disconnect", "drop"):
        emit("disconnected")
        break
