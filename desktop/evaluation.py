"""Measured hold evaluation. No clinical form score or prescribed exercise targets."""
import csv
import hashlib
import json
import math
import sqlite3
from datetime import datetime, timezone
from pathlib import Path

EXERCISES = {"wall_sit": "Wall sit", "plank": "Plank"}


def criteria_text(targets, minimum):
    return (f"Targets: wall sit {targets['wall_sit']:g}s; plank {targets['plank']:g}s.\n"
            f"Good requires BOTH: recorded in-band time reaches the target AND corrected consistency is at least {minimum:g}%.\n"
            "Corrected consistency = in-band time / (active time - final continuous out-of-band time) x 100.\n"
            "Earlier out-of-band periods still count against consistency when followed by a return in band.\n"
            "Pauses are excluded. The final departure (including final signal loss) is treated as exercise end.\n"
            "No valid in-band hold or missing ending data: Cannot evaluate.\n"
            "Missing echoes and slow sampling can reduce credited in-band time. This measures proximity to a reference, not full exercise form.\n"
            "Progress comparisons use these same criteria, the same exercise and the same tolerance. Import order is not the actual exercise date.")


def validate_criteria(targets, minimum):
    if set(targets) != set(EXERCISES) or any(not math.isfinite(v) or v <= 0 for v in targets.values()):
        raise ValueError("Enter a positive target duration for both exercises.")
    if not math.isfinite(minimum) or not 0 < minimum <= 100:
        raise ValueError("Consistency requirement must be above 0 and at most 100%.")


def normalize(row):
    exercise = row.get("exercise", "")
    if exercise not in EXERCISES:
        raise ValueError("Only wall-sit and plank summaries can be evaluated.")
    def integer(key):
        raw = row.get(key, "")
        value = int(raw)
        if value < 0: raise ValueError(f"{key} cannot be negative.")
        return value
    item = dict(session_id=integer("session_id"), exercise=exercise,
                active_ms=integer("active_ms"), in_band_ms=integer("in_band_ms"),
                reference_cm=float(row["reference_cm"]), band_cm=float(row["band_cm"]),
                trailing_out_ms=None, has_hold=None, history_id=None)
    if item["session_id"] < 1 or not 0 <= item["in_band_ms"] <= item["active_ms"]:
        raise ValueError("Invalid session ID or duration.")
    if any(not math.isfinite(item[k]) or item[k] < 0 for k in ("reference_cm", "band_cm")):
        raise ValueError("Invalid distance/tolerance.")
    if str(row.get("trailing_out_ms", "")).strip() not in ("", "None"):
        item["trailing_out_ms"] = integer("trailing_out_ms")
        item["has_hold"] = integer("has_hold")
        item["history_id"] = integer("history_id")
        if (item["has_hold"] not in (0, 1) or item["history_id"] < 1 or
                item["trailing_out_ms"] > item["active_ms"] or
                item["in_band_ms"] > item["active_ms"]-item["trailing_out_ms"] or
                (not item["has_hold"] and item["in_band_ms"] > 0)):
            raise ValueError("Invalid final-interval data.")
    return item


def load_csv(path):
    with Path(path).open(newline="", encoding="utf-8-sig") as source:
        rows = list(csv.DictReader(source))
    return [normalize(row) for row in rows]


def evaluate(row, targets, minimum):
    validate_criteria(targets, minimum)
    target = targets[row["exercise"]]
    active, inside = row["active_ms"]/1000, row["in_band_ms"]/1000
    result = dict(active_seconds=active, in_band_seconds=inside, target_seconds=target,
                  completion_percent=min(100, inside/target*100),
                  remaining_seconds=max(0, target-inside), consistency_percent=None,
                  adjusted_active_seconds=None, trailing_seconds=None,
                  uncredited_seconds=None, status="Cannot evaluate", reasons=[])
    if row["trailing_out_ms"] is None:
        result["reasons"] = ["This older record has no final-interval data. Its recorded hold time is shown, but corrected consistency is unavailable."]
        return result
    tail = row["trailing_out_ms"]/1000
    adjusted = active-tail
    result.update(adjusted_active_seconds=adjusted, trailing_seconds=tail,
                  uncredited_seconds=max(0, adjusted-inside))
    if not row["has_hold"] or inside <= 0 or adjusted <= 0:
        result["reasons"] = ["No valid credited hold was recorded. Check calibration, sensor placement and echo quality before repeating."]
        return result
    consistency = inside/adjusted*100
    result["consistency_percent"] = consistency
    if inside < target:
        result["reasons"].append(f"Recorded in-band time was {inside:.1f}s; {target-inside:.1f}s short of the {target:g}s target.")
    if consistency < minimum:
        result["reasons"].append(f"Consistency was {consistency:.1f}%; {minimum-consistency:.1f} percentage points below the {minimum:g}% requirement. Movement or missing echoes can lower this value.")
    result["status"] = "Good" if not result["reasons"] else "Not good yet"
    if not result["reasons"]:
        result["reasons"] = ["The recorded hold reached the target and met your confirmed consistency requirement."]
    return result


def record_key(row):
    return hashlib.sha256(json.dumps(row, sort_keys=True).encode()).hexdigest()


class ResultStore:
    def __init__(self, path):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(self.path)
        self.db.execute("CREATE TABLE IF NOT EXISTS sessions (participant TEXT, key TEXT, imported TEXT, payload TEXT, PRIMARY KEY(participant,key))")
        self.db.execute("CREATE TABLE IF NOT EXISTS settings (participant TEXT PRIMARY KEY, payload TEXT)")
        self.db.commit()
    def participants(self):
        rows = self.db.execute("SELECT participant FROM sessions UNION SELECT participant FROM settings ORDER BY participant").fetchall()
        return [r[0] for r in rows]
    def add(self, participant, rows):
        if not participant.strip(): raise ValueError("Enter a participant label.")
        count = 0
        with self.db:
            for row in rows:
                cur = self.db.execute("INSERT OR IGNORE INTO sessions VALUES (?,?,?,?)",
                                      (participant.strip(), record_key(row), datetime.now(timezone.utc).isoformat(), json.dumps(row)))
                count += cur.rowcount
        return count
    def load(self, participant):
        rows = self.db.execute("SELECT imported,payload FROM sessions WHERE participant=? ORDER BY rowid", (participant,)).fetchall()
        items=[]
        for index, (imported,payload) in enumerate(rows):
            item=json.loads(payload);item["imported_at_utc"]=imported;item["import_order"]=index
            items.append(item)
        # New-format records have an archive generation that survives resets and
        # increases on erase. Legacy records stay in their import order, unscored.
        return sorted(items, key=lambda r: (r["history_id"] is not None,
                                            r["history_id"] or 0,
                                            r["session_id"] if r["history_id"] else r["import_order"]))
    def save_criteria(self, participant, targets, minimum):
        validate_criteria(targets,minimum)
        with self.db:
            self.db.execute("INSERT OR REPLACE INTO settings VALUES (?,?)", (participant,json.dumps(dict(targets=targets, minimum=minimum))))
    def criteria(self, participant):
        row=self.db.execute("SELECT payload FROM settings WHERE participant=?",(participant,)).fetchone()
        return json.loads(row[0]) if row else None
    def close(self): self.db.close()


def progression(rows, current_index, targets, minimum):
    current=rows[current_index]
    compatible=[i for i in range(current_index) if rows[i]["exercise"]==current["exercise"] and
                abs(rows[i]["band_cm"]-current["band_cm"]) < 0.001 and
                evaluate(rows[i],targets,minimum)["consistency_percent"] is not None]
    now=evaluate(current,targets,minimum)
    if not compatible or now["consistency_percent"] is None: return None
    previous=evaluate(rows[compatible[-1]],targets,minimum)
    first=evaluate(rows[compatible[0]],targets,minimum)
    return dict(previous=previous, first=first, previous_index=compatible[-1],
                hold_change=now["in_band_seconds"]-previous["in_band_seconds"],
                consistency_change=now["consistency_percent"]-previous["consistency_percent"],
                first_hold_change=now["in_band_seconds"]-first["in_band_seconds"],
                first_consistency_change=now["consistency_percent"]-first["consistency_percent"])
