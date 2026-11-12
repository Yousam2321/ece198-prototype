"""Measured hold evaluation. No clinical form score or prescribed exercise targets."""
import csv
import hashlib
import json
import math
import sqlite3
from datetime import datetime, timezone
from pathlib import Path

EXERCISES = {"wall_sit": "Wall sit", "plank": "Plank", "chest_press": "Chest press"}


def criteria_text(targets, minimum, rep_minimum=90):
    goals=', '.join(f"{EXERCISES[k]} {v:g}{' good reps' if k=='chest_press' else 's'}" for k,v in targets.items())
    return (f"Targets: {goals}.\n\n"
            f"Wall sit / plank: Good requires BOTH the in-zone time target and at least {minimum:g}% consistency.\n"
            "Consistency = in-band time / (active time - final continuous departure) x 100. Pauses and final departure are excluded; earlier departures followed by a return still count.\n\n"
            f"Chest press: Good requires BOTH the good-rep target and at least {rep_minimum:g}% good reps.\n"
            "Good-rep percentage = good reps / completed attempts x 100. Short attempts count against it. Unfinished reps discarded by the firmware do not count as attempts. Zero attempts: Cannot evaluate.\n"
            "Each good rep reaches 80% of calibrated travel and returns within 20% of the start. Extra travel is allowed; there is no upper-travel penalty.\n\n"
            "No valid hold or missing ending data: Cannot evaluate. Missing echoes may reduce credited results. These are distance measures, not a full-body form or medical grade.\n"
            "Comparisons use the current confirmed goals. Changed chest calibration is flagged. Attempt order is not a calendar date.")


def validate_criteria(targets, minimum, rep_minimum=90):
    if not targets or not set(targets).issubset(EXERCISES) or any(not math.isfinite(v) or v <= 0 for v in targets.values()):
        raise ValueError("Enter a positive target for the exercises you want to evaluate.")
    if 'chest_press' in targets and (not float(targets['chest_press']).is_integer() or targets['chest_press']>65535):
        raise ValueError("Chest-press target must be a whole number from 1 to 65535 good reps.")
    for value,label in ((minimum,'Consistency'),(rep_minimum,'Good-rep percentage')):
        if not math.isfinite(value) or not 0 < value <= 100:
            raise ValueError(f"{label} requirement must be above 0 and at most 100%.")


def normalize_rep(row):
    item={key:int(row[key]) for key in ('session_id','active_ms','good_reps','attempts','history_id')}
    item.update({key:float(row[key]) for key in ('start_cm','pressed_cm','last_travel_cm')})
    item['exercise']='chest_press'
    travel=abs(item['pressed_cm']-item['start_cm'])
    if (item['session_id']<1 or item['history_id']<1 or not 0<=item['active_ms']<=4294967295 or
        not 0<=item['good_reps']<=item['attempts']<=65535 or
        any(not math.isfinite(item[k]) for k in ('start_cm','pressed_cm','last_travel_cm')) or
        not 12<=item['start_cm']<=150 or not 12<=item['pressed_cm']<=150 or travel<10 or item['last_travel_cm']<0):
        raise ValueError('Invalid chest-press summary. Check the exported measurements/counts.')
    return item


def normalize(row):
    exercise = row.get("exercise", "")
    if exercise=="chest_press":return normalize_rep(row)
    if exercise not in EXERCISES:
        raise ValueError("Only wall-sit, plank and chest-press summaries can be evaluated.")
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
        reader=csv.DictReader(source)
        headers=set(reader.fieldnames or [])
        if 'active_seconds' in headers and 'active_ms' not in headers:
            raise ValueError('This is an older evaluated report. Open the original Arduino saved_sessions CSV instead. New evaluated exports can be re-imported.')
        rows=list(reader)
    try:return [normalize(row) for row in rows]
    except KeyError as error:
        raise ValueError(f'Missing result column: {error.args[0]}. Open the original saved-session CSV.') from None


def evaluate(row, targets, minimum, rep_minimum=90):
    validate_criteria(targets, minimum, rep_minimum)
    if row['exercise']=='chest_press':return evaluate_reps(row,targets,rep_minimum)
    if row['exercise'] not in targets:
        return dict(kind='hold',status='Cannot evaluate',missing_target=True,
                    reasons=['Set and confirm a target for this exercise.'],consistency_percent=None,
                    in_band_seconds=row['in_band_ms']/1000,active_seconds=row['active_ms']/1000,
                    target_seconds=None,trailing_seconds=None)
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


def evaluate_reps(row, targets, minimum):
    target=targets.get('chest_press')
    attempts=row['attempts'];good=row['good_reps'];travel=abs(row['pressed_cm']-row['start_cm'])
    result=dict(kind='reps',status='Cannot evaluate',active_seconds=row['active_ms']/1000,
                good_reps=good,attempts=attempts,short_attempts=attempts-good,
                good_rep_percent=100*good/attempts if attempts else None,
                target_reps=target,remaining_reps=max(0,target-good) if target else None,
                calibrated_travel_cm=travel,required_travel_cm=travel*.8,return_tolerance_cm=travel*.2,
                minimum_percent=minimum,reasons=[],missing_target=target is None)
    if target is None:result['reasons']=['Set and confirm a chest-press rep target.'];return result
    if not attempts:result['reasons']=['No completed attempts were recorded. Repeat after checking calibration.'];return result
    if good<target:result['reasons'].append(f'{target-good:g} more good rep'+('s' if target-good!=1 else '')+f' needed to reach {target:g}.')
    if result['good_rep_percent']<minimum:
        result['reasons'].append(f"Good reps were {result['good_rep_percent']:.1f}%; need {minimum:g}%.")
    result['status']='Not good yet' if result['reasons'] else 'Good'
    if not result['reasons']:result['reasons']=['Both the good-rep count and good-rep percentage goals were met.']
    return result


def attempt_number(rows,index):
    return sum(r['exercise']==rows[index]['exercise'] for r in rows[:index+1])


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
    def participants(self, limit=100):
        return [r["participant"] for r in self.participant_summaries(limit=limit)]
    def participant_summaries(self, query="", limit=100):
        # Bound the picker/manager list even with thousands of profiles.
        pattern="%"+query.strip().replace("\\","\\\\").replace("%","\\%").replace("_","\\_")+"%"
        rows=self.db.execute("""WITH people AS (
            SELECT participant FROM sessions UNION SELECT participant FROM settings
        ), totals AS (SELECT participant,COUNT(*) AS attempts FROM sessions GROUP BY participant)
        SELECT p.participant,COALESCE(t.attempts,0) FROM people p
        LEFT JOIN totals t ON p.participant=t.participant
        WHERE p.participant LIKE ? ESCAPE '\\'
        ORDER BY p.participant COLLATE NOCASE LIMIT ?""",(pattern,max(1,min(int(limit),1000)))).fetchall()
        return [dict(participant=name,attempts=count) for name,count in rows]
    def participant_count(self):
        return self.db.execute("SELECT COUNT(*) FROM (SELECT participant FROM sessions UNION SELECT participant FROM settings)").fetchone()[0]
    def delete_participant(self, participant):
        if not participant.strip(): raise ValueError("Select a participant first.")
        with self.db:
            count=self.db.execute("DELETE FROM sessions WHERE participant=?",(participant,)).rowcount
            self.db.execute("DELETE FROM settings WHERE participant=?",(participant,))
        return count
    def delete_row(self, participant, row):
        # Use the exact persisted key, not the displayed row number. Board IDs
        # can overlap between the hold/rep archives and between participants.
        key=row.get('_record_key') or record_key(normalize(row))
        with self.db:
            count=self.db.execute('DELETE FROM sessions WHERE participant=? AND key=?',(participant,key)).rowcount
            if count:
                # A profile with no criteria must survive deletion of its last row.
                self.db.execute('INSERT OR IGNORE INTO settings VALUES (?,?)',(participant,'{}'))
        return count
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
        rows = self.db.execute("SELECT key,imported,payload FROM sessions WHERE participant=? ORDER BY rowid", (participant,)).fetchall()
        items=[]
        for index, (key,imported,payload) in enumerate(rows):
            item=json.loads(payload);item["_record_key"]=key;item["imported_at_utc"]=imported;item["import_order"]=index
            items.append(item)
        # New-format records have an archive generation that survives resets and
        # increases on erase. Legacy records stay in their import order, unscored.
        return sorted(items, key=lambda r: (r["history_id"] is not None,
                                            r["history_id"] or 0,
                                            r["session_id"] if r["history_id"] else r["import_order"]))
    def save_criteria(self, participant, targets, minimum, rep_minimum=90):
        validate_criteria(targets,minimum,rep_minimum)
        with self.db:
            self.db.execute("INSERT OR REPLACE INTO settings VALUES (?,?)", (participant,json.dumps(dict(targets=targets, minimum=minimum, rep_minimum=rep_minimum))))
    def criteria(self, participant):
        row=self.db.execute("SELECT payload FROM settings WHERE participant=?",(participant,)).fetchone()
        return json.loads(row[0]) if row else None
    def close(self): self.db.close()


def progression(rows, current_index, targets, minimum, rep_minimum=90):
    current=rows[current_index]
    if current['exercise']=='chest_press':
        now=evaluate(current,targets,minimum,rep_minimum)
        earlier=[i for i in range(current_index) if rows[i]['exercise']=='chest_press' and
                 evaluate(rows[i],targets,minimum,rep_minimum)['good_rep_percent'] is not None]
        if not earlier or now['status']=='Cannot evaluate':return None
        previous=evaluate(rows[earlier[-1]],targets,minimum,rep_minimum)
        first=evaluate(rows[earlier[0]],targets,minimum,rep_minimum)
        return dict(kind='reps',previous=previous,first=first,previous_index=earlier[-1],
                    good_change=now['good_reps']-previous['good_reps'],
                    percent_change=now['good_rep_percent']-previous['good_rep_percent'],
                    first_good_change=now['good_reps']-first['good_reps'],
                    first_percent_change=now['good_rep_percent']-first['good_rep_percent'],
                    range_changed=abs(now['calibrated_travel_cm']-previous['calibrated_travel_cm'])>.1,
                    first_range_changed=abs(now['calibrated_travel_cm']-first['calibrated_travel_cm'])>.1)

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
