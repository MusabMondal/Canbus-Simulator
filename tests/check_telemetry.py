"""Check the complete encode/deliver/log pipeline across braking and counter wrap."""
import csv
import sys
with open(sys.argv[1], newline="") as stream:
    rows = list(csv.DictReader(stream))
assert len(rows) == 1000
for i, row in enumerate(rows):
    assert int(row["Timestamp"]) == i * 10
    assert 0 <= int(row["RPM"]) <= 7000
    for key in ("Throttle", "BrakePressure"):
        assert 0 <= int(row[key]) <= 100
    assert int(row["EngineOn"]) == 1
assert int(rows[105]["BrakePressure"]) == 100
assert int(rows[220]["BrakePressure"]) == 0
assert any(int(row["Speed"]) > 20 for row in rows)
print("1000-step telemetry checks passed")
