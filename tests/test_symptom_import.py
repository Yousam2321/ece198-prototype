from pathlib import Path
import csv
import sqlite3
import sys
import tempfile
import unittest
from contextlib import closing

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "desktop"))
from test_export_sessions import Port
from importing import read_all_history
from evaluation import ResultStore, load_csv
from clean_report import build_report
from report_data import export_assessment_csv


class SymptomImportTest(unittest.TestCase):
    def test_import_both_boards_into_one_profile(self):
        a = Port([b"# ASAVED,id,exercise,active_ms,reps,bpm_avg,bpm_samples,history_id\n",
                  b"A_UNWELL,1,2,1\n", b"# No saved A sessions\n"])
        b = Port([b"# SAVED,id,exercise,active_ms,in_band_ms\n",
                  b"UNWELL,1,1,0,1\n", b"# No saved sessions\n",
                  b"# REPSAVED,id,exercise\n", b"# No saved rep sessions\n"])
        snapshots = []
        for port in (a, b):
            snapshot = {}
            rows, _ = read_all_history(port, symptom_snapshot=snapshot, symptom_source="port:COM10",
                                       timeout=2, clock=port.clock)
            self.assertEqual(rows, [])
            snapshots.append(snapshot)
        with tempfile.TemporaryDirectory() as temp:
            store = ResultStore(Path(temp) / "results.sqlite")
            for snapshot in snapshots + snapshots:
                store.add("Combined participant", [], snapshot)
            counts = store.symptom_counts("Combined participant")
            self.assertEqual(counts, {"squats": 2, "lateral_raises": 1, "wall_sit": 1, "chest_press": 1})
            self.assertEqual(sum(counts.values()), 5)
            self.assertEqual(store.load("Combined participant"), [])
            report = Path(temp) / "combined.pdf"
            build_report(report, "Combined participant", [], {}, 90, symptoms=counts)
            self.assertTrue(report.read_bytes().startswith(b"%PDF"))
            csv_path = Path(temp) / "combined.csv"
            export_assessment_csv(csv_path, "Combined participant", [], {}, 90, symptoms=counts)
            with csv_path.open(encoding="utf-8-sig", newline="") as saved:
                summary = list(csv.DictReader(saved))
            self.assertTrue(all(row["row_type"] == "unwell_summary" for row in summary))
            self.assertEqual({row["exercise"]: int(row["times_unwell"]) for row in summary}, counts)
            self.assertEqual(load_csv(csv_path), [])
            store.close()

    def test_symptoms_only_import_and_pdf(self):
        port = Port([b"# SAVED,id,exercise,active_ms,in_band_ms\n",
                     b"UNWELL,1,1,0,2\n", b"# No saved sessions\n",
                     b"# REPSAVED,id,exercise\n", b"# No saved rep sessions\n"])
        snapshot = {"source": "test-station"}
        rows, _ = read_all_history(port, symptom_snapshot=snapshot, timeout=2, clock=port.clock)
        self.assertEqual(rows, [])
        with tempfile.TemporaryDirectory() as temp:
            store = ResultStore(Path(temp) / "results.sqlite")
            self.assertEqual(store.pending_symptoms("Participant 1", snapshot), {"wall_sit": 1, "chest_press": 2})
            store.add("Participant 1", rows, snapshot)
            store.add("Participant 1", rows, snapshot)
            store.add("Participant 2", rows, snapshot)
            self.assertEqual(store.symptom_counts("Participant 1"), {"wall_sit": 1, "chest_press": 2})
            self.assertEqual(store.symptom_counts("Participant 2"), {"wall_sit": 1, "chest_press": 2})
            snapshot["counts"]["plank"] = 1
            store.add("Participant 2", rows, snapshot)
            self.assertEqual(store.symptom_counts("Participant 2"), {"plank": 1, "wall_sit": 1, "chest_press": 2})
            snapshot["generation"] = 2
            snapshot["counts"] = {"wall_sit": 0, "plank": 0, "chest_press": 0}
            self.assertEqual(store.pending_symptoms("Participant 2", snapshot), {})
            snapshot["counts"]["chest_press"] = 1
            store.add("Participant 2", rows, snapshot)
            self.assertEqual(store.symptom_counts("Participant 2"), {"plank": 1, "wall_sit": 1, "chest_press": 3})
            self.assertEqual(store.participant_count(), 2)
            report = Path(temp) / "report.pdf"
            build_report(report, "Participant 2", [], {}, 90, symptoms=store.symptom_counts("Participant 2"))
            self.assertTrue(report.read_bytes().startswith(b"%PDF"))
            self.assertEqual(store.load("Participant 2"), [])
            store.close()

    def test_existing_reports_and_repeat_imports_after_upgrade(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "legacy.sqlite"
            with closing(sqlite3.connect(path)) as db, db:
                db.executescript("""
                    CREATE TABLE symptom_reports (participant TEXT, exercise TEXT, count INTEGER,
                        PRIMARY KEY(participant,exercise));
                    CREATE TABLE symptom_imports (source TEXT, generation INTEGER, exercise TEXT, count INTEGER,
                        PRIMARY KEY(source,generation,exercise));
                    INSERT INTO symptom_reports VALUES ('First','squats',1),('First','lateral_raises',1);
                    INSERT INTO symptom_imports VALUES ('part_a:port:COM6',1,'squats',1),
                                                       ('part_a:port:COM6',1,'lateral_raises',1);
                """)
            snapshot = dict(source="part_a:port:COM6", generation=1, counts={"squats":1,"lateral_raises":1})
            store = ResultStore(path)
            self.assertEqual(store.pending_symptoms("First", snapshot), {})
            self.assertEqual(store.pending_symptoms("Second", snapshot), snapshot["counts"])
            store.add("Second", [], snapshot)
            store.close()
            store = ResultStore(path)
            for participant in ("First", "Second"):
                store.add(participant, [], snapshot)
                self.assertEqual(store.symptom_counts(participant), {"squats":1,"lateral_raises":1})
            snapshot["counts"]["squats"] = 2
            store.add("Second", [], snapshot)
            self.assertEqual(store.symptom_counts("Second"), {"squats":2,"lateral_raises":1})
            self.assertEqual(store.symptom_counts("First"), {"squats":1,"lateral_raises":1})
            store.delete_participant("Second")
            self.assertEqual(store.pending_symptoms("Second", snapshot), snapshot["counts"])
            store.close()
