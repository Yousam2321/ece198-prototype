"""Chest import/scoring/persistence/report tests, with no hardware or Tk required."""
import sys,csv,json,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'desktop'),str(ROOT/'tools')]
import evaluation as e
from importing import read_all_history
from report_data import export_assessment_csv,preview_values
from presentation import result_label,short_feedback
from clean_report import build_report
from test_export_sessions import Port
from test_evaluation import row

TARGETS={'wall_sit':25,'plank':25,'chest_press':10}
def rep(id=1,good=9,attempts=12,end=45.2,**kwargs):
    return e.normalize(dict(session_id=id,exercise='chest_press',active_ms=60517,good_reps=good,
        attempts=attempts,start_cm=20.7,pressed_cm=end,last_travel_cm=43.8,history_id=1,**kwargs))

class ChestAppTests(unittest.TestCase):
    def test_score_and_specific_reasons(self):
        r=e.evaluate(rep(),TARGETS,90,90)
        self.assertEqual(r['good_rep_percent'],75);self.assertEqual(r['short_attempts'],3)
        self.assertEqual(r['remaining_reps'],1);self.assertAlmostEqual(r['required_travel_cm'],19.6)
        self.assertIn('rep count + good-rep %',result_label(r,90))
        self.assertIn('1 more good rep',short_feedback(r,90))
        self.assertEqual(e.evaluate(rep(good=10,attempts=10),TARGETS,90,90)['status'],'Good')
        self.assertIn('good-rep %',result_label(e.evaluate(rep(good=10,attempts=12),TARGETS,90,90),90))
        self.assertEqual(result_label(e.evaluate(rep(good=9,attempts=9),TARGETS,90,90),90),'Not good yet - rep count')
    def test_zero_missing_target_and_invalid_input(self):
        r=e.evaluate(rep(good=0,attempts=0),TARGETS,90,90)
        self.assertEqual(r['status'],'Cannot evaluate');self.assertIsNone(r['good_rep_percent'])
        self.assertTrue(e.evaluate(rep(),{'plank':25},90)['missing_target'])
        for change in ({'good_reps':13},{'attempts':-1},{'start_cm':float('nan')},{'pressed_cm':22}):
            source=dict(rep());source.update(change)
            with self.assertRaises(ValueError):e.normalize(source)
        with self.assertRaises(ValueError):e.validate_criteria({'chest_press':1.5},90)
    def test_travel_overshoot_allowed_and_range_changes_flagged(self):
        r=e.evaluate(rep(good=12,attempts=12),TARGETS,90,90)
        self.assertEqual(r['status'],'Good') # Last travel greater than calibration is allowed.
        change=e.progression([rep(),rep(id=2,end=54.9)],1,TARGETS,90,90)
        self.assertTrue(change['range_changed']);self.assertEqual(change['good_change'],0)
    def test_csv_roundtrip_ids_and_old_profile_preserved(self):
        with tempfile.TemporaryDirectory() as d:
            db=Path(d)/'history.sqlite3';store=e.ResultStore(db)
            old=row();store.add('P',[old]);store.save_criteria('P',{'wall_sit':25,'plank':25},90)
            store.close();store=e.ResultStore(db)
            self.assertEqual(len(store.load('P')),1)
            chest=rep();self.assertEqual(store.add('P',[chest]),1) # Same board ID/history, different exercise.
            self.assertEqual(store.add('P',[e.normalize(chest)]),0)
            store.save_criteria('P',TARGETS,90,75);self.assertEqual(store.criteria('P')['rep_minimum'],75)
            path=Path(d)/'results.csv';export_assessment_csv(path,'P',store.load('P'),TARGETS,90,75)
            imported=e.load_csv(path);self.assertEqual(len(imported),2)
            self.assertEqual(store.add('P',imported),0)
            self.assertEqual(preview_values(chest)[3],'9 / 12 reps')
            store.delete_participant('P');self.assertFalse(store.participants());store.close()
    def test_both_archives_and_old_firmware(self):
        header=b'# SAVED,id,exercise,active_ms,in_band_ms,reference_cm,band_cm\n'
        hold=b'SAVED,1,plank,35000,27000,50,10,5000,1,1\n'
        rep_header=b'# REPSAVED,id,exercise,active_ms,good_reps,attempts,start_cm,pressed_cm,last_travel_cm,history_id\n'
        raw=b'REPSAVED,1,chest_press,60517,9,12,20.7,45.2,43.8,1\n'
        p=Port([header,hold,b'# END_HISTORY\n',rep_header,raw,b'# END_REP_HISTORY\n'])
        rows,notice=read_all_history(p,timeout=2,clock=p.clock)
        self.assertEqual(len(rows),2);self.assertEqual(p.sent,b'history\nrep_history\n');self.assertEqual(notice,'')
        p=Port([header,hold,b'# END_HISTORY\n',b'EVENT,station,command_not_available\n'])
        rows,notice=read_all_history(p,timeout=2,clock=p.clock);self.assertEqual(len(rows),1);self.assertIn('Older',notice)
        p=Port([header,hold,b'# END_HISTORY\n',rep_header,raw])
        with self.assertRaises(TimeoutError):read_all_history(p,timeout=2,clock=p.clock)
    def test_mixed_and_chest_only_pdf(self):
        with tempfile.TemporaryDirectory() as d:
            for name,rows,targets in [('mixed',[row(),rep(),rep(2,end=54.9)],TARGETS),
                                     ('chest',[rep(good=0,attempts=0)],{'chest_press':10})]:
                path=Path(d)/(name+'.pdf');build_report(path,'Synthetic',rows,targets,90,90)
                self.assertGreater(path.stat().st_size,5000)
                with self.assertRaises(FileExistsError):build_report(path,'P',rows,targets,90,90)

if __name__=='__main__':unittest.main()
