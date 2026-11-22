from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "desktop"))
from test_export_sessions import Port
from importing import read_all_history
from evaluation import ResultStore
from clean_report import build_report


class SymptomImportTest(unittest.TestCase):
    def test_symptoms_only_import_and_pdf(self):
        port = Port([b"# SAVED,id,exercise,active_ms,in_band_ms\n",
                     b"UNWELL,1,1,0,2\n", b"# No saved sessions\n",
                     b"# REPSAVED,id,exercise\n", b"# No saved rep sessions\n"])
        snapshot = {"source": "test-station"}
        rows, _ = read_all_history(port, symptom_snapshot=snapshot, timeout=2, clock=port.clock)
        self.assertEqual(rows, [])
        with tempfile.TemporaryDirectory() as temp:
            store = ResultStore(Path(temp) / "results.sqlite")
            self.assertEqual(store.pending_symptoms(snapshot), {"wall_sit": 1, "chest_press": 2})
            store.add("Participant 1", rows, snapshot)
            store.add("Participant 1", rows, snapshot)
            store.add("Participant 2", rows, snapshot)
            self.assertEqual(store.symptom_counts("Participant 1"), {"wall_sit": 1, "chest_press": 2})
            self.assertEqual(store.symptom_counts("Participant 2"), {})
            snapshot["counts"]["plank"] = 1
            store.add("Participant 2", rows, snapshot)
            self.assertEqual(store.symptom_counts("Participant 2"), {"plank": 1})
            snapshot["generation"] = 2
            snapshot["counts"] = {"wall_sit": 0, "plank": 0, "chest_press": 0}
            self.assertEqual(store.pending_symptoms(snapshot), {})
            snapshot["counts"]["chest_press"] = 1
            store.add("Participant 2", rows, snapshot)
            self.assertEqual(store.symptom_counts("Participant 2"), {"plank": 1, "chest_press": 1})
            self.assertEqual(store.participant_count(), 2)
            report = Path(temp) / "report.pdf"
            build_report(report, "Participant 2", [], {}, 90, symptoms=store.symptom_counts("Participant 2"))
            self.assertTrue(report.read_bytes().startswith(b"%PDF"))
            self.assertEqual(store.load("Participant 2"), [])
            store.close()
