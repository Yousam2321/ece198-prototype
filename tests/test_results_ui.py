"""Data safety and readable output checks for the desktop-only update."""
import sys
from pathlib import Path
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'desktop'))
from evaluation import ResultStore, evaluate
from presentation import result_label, short_feedback
from clean_report import build_report
from test_evaluation import row, TARGETS

class ResultsUiTests(unittest.TestCase):
    def test_all_failure_reasons_and_missing_metadata(self):
        cases=[(row(inside=24000), 'time + consistency'),
               (row(active=25000,inside=20000,tail=5000), 'time'),
               (row(active=40000,inside=28000), 'consistency'),
               (row(inside=29000), 'both goals met')]
        for item,expected in cases:
            r=evaluate(item,TARGETS,90)
            self.assertIn(expected,result_label(r,90))
            self.assertTrue(short_feedback(r,90))
        legacy=row();legacy['trailing_out_ms']=None
        self.assertIn('missing ending data',result_label(evaluate(legacy,TARGETS,90),90))

    def test_delete_removes_entire_profile_and_preserves_others(self):
        with tempfile.TemporaryDirectory() as directory:
            store=ResultStore(Path(directory)/'results.sqlite3')
            for person in ('P1000','P1001'):
                store.add(person,[row()]);store.save_criteria(person,TARGETS,90)
            self.assertEqual(store.delete_participant('P1000'),1)
            self.assertEqual(store.participants(),['P1001'])
            self.assertEqual(store.load('P1000'),[]);self.assertIsNone(store.criteria('P1000'))
            self.assertEqual(len(store.load('P1001')),1)
            self.assertIsNotNone(store.criteria('P1001'))
            store.save_criteria('Empty profile',TARGETS,90)
            self.assertEqual(store.delete_participant('Empty profile'),0)
            self.assertNotIn('Empty profile',store.participants())
            store.close()
            reopened=ResultStore(Path(directory)/'results.sqlite3')
            self.assertEqual(reopened.participants(),['P1001']);reopened.close()

    def test_search_and_bounded_picker_for_ten_thousand_profiles(self):
        with tempfile.TemporaryDirectory() as directory:
            store=ResultStore(Path(directory)/'results.sqlite3')
            with store.db:
                store.db.executemany('INSERT INTO settings VALUES (?,?)',[(f'P{i:05d}','{}') for i in range(10000)])
            self.assertEqual(store.participant_count(),10000)
            self.assertEqual(len(store.participants()),100)
            self.assertEqual(store.participant_summaries('P09999'),[dict(participant='P09999',attempts=0)])
            for label in ('a%b','a_b',r'a\b'):
                store.save_criteria(label,TARGETS,90)
                self.assertEqual([r['participant'] for r in store.participant_summaries(label)],[label])
                store.delete_participant(label)
            self.assertEqual(store.participant_summaries("' OR 1=1 --"),[])
            store.delete_participant('P09999')
            self.assertEqual(store.participant_count(),9999)
            self.assertFalse(store.participant_summaries('P09999'));store.close()

    def test_pdf_grouping_continuation_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            rows=[row(i,inside=24000 if i%2 else 29000) for i in range(1,19)]
            rows.extend([row(19,exercise='plank'),row(20,band=4)])
            legacy=row(21);legacy['trailing_out_ms']=None;rows.append(legacy)
            path=Path(directory)/'report.pdf'
            build_report(path,'Synthetic participant',rows,TARGETS,90)
            self.assertTrue(path.read_bytes().startswith(b'%PDF-'))
            self.assertGreater(path.stat().st_size,5000)
            original=path.read_bytes()
            with self.assertRaises(FileExistsError):build_report(path,'Another',rows,TARGETS,90)
            self.assertEqual(path.read_bytes(),original)
            with self.assertRaises(ValueError):build_report(Path(directory)/'empty.pdf','P',[],TARGETS,90)

if __name__=='__main__':unittest.main()
