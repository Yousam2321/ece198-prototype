"""Part A scoring, serial import, profile migration and reports."""
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'desktop'),str(ROOT/'tools')]
import evaluation as e
from importing import read_all_history
from presentation import result_label,short_feedback,progress_summary
from report_data import export_assessment_csv,preview_values
from clean_report import build_report
from test_evaluation import row
from test_chest_app import rep
from test_part_a_export import HEADER,ROW,NO_BPM,END
from test_export_sessions import Port

TARGETS={'wall_sit':25,'plank':25,'chest_press':10,'squats':5,'lateral_raises':8}
def count(id=1,exercise='squats',reps=5,bpm=None,samples=0,**kwargs):
    return e.normalize(dict(session_id=id,exercise=exercise,active_ms=20000,reps=reps,
                            bpm_avg=bpm,bpm_samples=samples,history_id=1,**kwargs))

class PartAAppTests(unittest.TestCase):
    def test_count_criteria_do_not_fabricate_quality(self):
        a=e.evaluate(count(),TARGETS,90,90)
        self.assertEqual(result_label(a,90),'Good - rep target met')
        self.assertNotIn('good_rep_percent',a);self.assertNotIn('consistency_percent',a)
        self.assertIsNone(a['bpm_avg']);self.assertEqual(short_feedback(a,90),'Recorded rep target met.')
        a=e.evaluate(count(reps=3),TARGETS,90,90)
        self.assertEqual(a['remaining_reps'],2);self.assertIn('2 more reps',short_feedback(a,90))
        self.assertEqual(e.evaluate(count(reps=0),TARGETS,90)['status'],'Not good yet')
        self.assertEqual(result_label(e.evaluate(count(),{'plank':25},90),90),'Not scored - set target')
        for target in (0,1.5,32768,float('nan')):
            with self.assertRaises(ValueError):e.validate_criteria({'squats':target},90)

    def test_mean_bpm_does_not_change_score(self):
        for bpm,samples in ((None,0),(40,3),(180,3)):
            self.assertEqual(e.evaluate(count(bpm=bpm,samples=samples),TARGETS,90)['status'],'Good')
        for change in ({'bpm_avg':0,'bpm_samples':4},{'bpm_avg':72,'bpm_samples':0},
                       {'bpm_avg':float('nan'),'bpm_samples':3},{'reps':-1}):
            raw=count();raw.update(change)
            with self.assertRaises(ValueError):e.normalize(raw)

    def test_direct_read_only_sends_the_part_a_command(self):
        p=Port([HEADER,ROW,NO_BPM,END])
        rows,notice=read_all_history(p,timeout=2,clock=p.clock)
        self.assertEqual(p.sent,b'history\n');self.assertEqual(notice,'')
        self.assertEqual([r['exercise'] for r in rows],['squats','lateral_raises'])
        self.assertEqual(rows[0]['bpm_avg'],72.3);self.assertIsNone(rows[1]['bpm_avg'])

    def test_existing_profiles_duplicate_imports_and_row_deletion(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'results.sqlite3';store=e.ResultStore(path)
            old=[row(),rep()];store.add('P',old);store.save_criteria('P',{'wall_sit':25,'chest_press':10},90)
            store.close();store=e.ResultStore(path)
            self.assertEqual(len(store.load('P')),2);self.assertNotIn('squats',store.criteria('P')['targets'])
            self.assertEqual(store.add('P',[count(),count(exercise='lateral_raises')]),2)
            self.assertEqual(store.add('P',[count()]),0)
            store.save_criteria('P',TARGETS,90)
            rows=store.load('P');self.assertEqual([r['exercise'] for r in rows],['wall_sit','chest_press','squats','lateral_raises'])
            exported=Path(d)/'evaluated.csv';export_assessment_csv(exported,'P',rows,TARGETS,90)
            imported=e.load_csv(exported);self.assertEqual(store.add('P',imported),0)
            selected=next(r for r in rows if r['exercise']=='squats')
            self.assertEqual(store.delete_row('P',selected),1)
            self.assertEqual(len(store.load('P')),3);self.assertEqual(store.criteria('P')['targets']['squats'],5)
            self.assertIn('P',store.participants());store.close()

    def test_progression_only_compares_the_same_exercise(self):
        rows=[count(reps=2),row(),count(2,reps=7),count(exercise='lateral_raises')]
        change=e.progression(rows,2,TARGETS,90)
        self.assertEqual(change['rep_change'],5)
        self.assertIn('reps +5',progress_summary(rows,2,change))
        self.assertIsNone(e.progression(rows,3,TARGETS,90))
        self.assertEqual(preview_values(rows[0])[3:6],('2 reps','No BPM','Count only'))

    def test_all_five_exercises_and_count_only_reports(self):
        with tempfile.TemporaryDirectory() as d:
            rows=[row(),row(exercise='plank'),rep(),count(bpm=72.3,samples=4),
                  count(2,reps=7),count(exercise='lateral_raises',reps=0)]
            for name,items,targets in (('all',rows,TARGETS),('only_a',rows[3:],{'squats':5,'lateral_raises':8})):
                path=Path(d)/(name+'.pdf');build_report(path,'Synthetic',items,targets,90)
                self.assertTrue(path.read_bytes().startswith(b'%PDF-'))
                self.assertGreater(path.stat().st_size,5000)
                with self.assertRaises(FileExistsError):build_report(path,'Synthetic',items,targets,90)

if __name__=='__main__':unittest.main()

