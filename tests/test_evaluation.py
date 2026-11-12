import importlib.util
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location("evaluation",Path(__file__).resolve().parents[1]/"desktop/evaluation.py")
e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e)
TARGETS={"wall_sit":25.0,"plank":25.0}

def row(id=1,active=35000,inside=27000,tail=5000,exercise="wall_sit",band=10,history=1):
    return e.normalize(dict(session_id=id,exercise=exercise,active_ms=active,in_band_ms=inside,
                            reference_cm=50,band_cm=band,trailing_out_ms=tail,has_hold=1,history_id=history))

class EvaluationTests(unittest.TestCase):
    def test_final_exit_removed_but_middle_gap_kept(self):
        r=e.evaluate(row(),TARGETS,90)
        self.assertEqual(r['consistency_percent'],90)
        self.assertEqual(r['adjusted_active_seconds'],30)
        self.assertEqual(r['status'],'Good')
    def test_short_hold_not_good(self):
        r=e.evaluate(row(active=15000,inside=10000,tail=5000),TARGETS,90)
        self.assertEqual(r['consistency_percent'],100)
        self.assertEqual(r['status'],'Not good yet')
        self.assertEqual(r['remaining_seconds'],15)
    def test_middle_outside_penalty(self):
        r=e.evaluate(row(inside=20000),TARGETS,90)
        self.assertEqual(r['status'],'Not good yet');self.assertEqual(len(r['reasons']),2)
    def test_legacy_unknown_never_guessed(self):
        source=dict(session_id=1,exercise='plank',active_ms=30000,in_band_ms=25000,reference_cm=43.9,band_cm=10)
        r=e.evaluate(e.normalize(source),TARGETS,90)
        self.assertEqual(r['status'],'Cannot evaluate');self.assertIsNone(r['consistency_percent'])
    def test_no_valid_hold(self):
        source=row(active=10000,inside=0,tail=0);source['has_hold']=0
        self.assertEqual(e.evaluate(source,TARGETS,90)['status'],'Cannot evaluate')
    def test_corrupt_interval(self):
        with self.assertRaises(ValueError):row(active=10000,inside=9000,tail=5000)
    def test_criteria_validation(self):
        for bad in (0,-1,float('nan')):
            with self.assertRaises(ValueError): e.validate_criteria({'wall_sit':bad,'plank':25},90)
    def test_store_reopen_dedup_people_and_generations(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'results.sqlite3';store=e.ResultStore(path)
            self.assertEqual(store.add('A',[row()]),1)
            self.assertEqual(store.add('A',[row()]),0)
            self.assertEqual(store.add('A',[row(history=2)]),1)
            store.add('B',[row()]);store.save_criteria('A',TARGETS,90);store.close()
            reopened=e.ResultStore(path)
            self.assertEqual(len(reopened.load('A')),2);self.assertEqual(len(reopened.load('B')),1)
            self.assertEqual(reopened.criteria('A')['minimum'],90);reopened.close()
    def test_comparison_same_exercise_and_tolerance(self):
        rows=[row(1,inside=26000),row(2,inside=28000,exercise='plank'),row(3,inside=28000,band=4),row(4,inside=29000)]
        p=e.progression(rows,3,TARGETS,90)
        self.assertEqual(p['previous_index'],0);self.assertEqual(p['hold_change'],3)
        self.assertAlmostEqual(p['consistency_change'],10)

if __name__=='__main__':unittest.main()
