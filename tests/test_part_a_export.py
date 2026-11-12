"""Part A history protocol, including missing beats and interrupted transfers."""
import csv
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_sessions as export
from test_export_sessions import Port

HEADER=b'# ASAVED,id,exercise,active_ms,reps,bpm_avg,bpm_samples,history_id\n'
ROW=b'ASAVED,1,squats,12345,5,72.3,4,1\n'
NO_BPM=b'ASAVED,2,lateral_raises,20000,0,,0,1\n'
END=b'# END_A_HISTORY\n'

class PartAExportTests(unittest.TestCase):
    def read(self,lines):
        p=Port(lines);metadata={}
        result=export.read_history(p,timeout=2,clock=p.clock,metadata=metadata)
        self.assertEqual(p.sent,b'history\n');self.assertEqual(metadata['device'],'part_a')
        return result

    def test_auto_detection_and_optional_bpm(self):
        rows=self.read([b'Z: 0 | Y: 0\n',HEADER,ROW,NO_BPM,END])
        self.assertEqual(rows[0]['reps'],5);self.assertEqual(rows[0]['bpm_avg'],'72.3')
        self.assertEqual(rows[0]['active_seconds'],'12.345')
        self.assertEqual(rows[1]['reps'],0);self.assertEqual(rows[1]['bpm_avg'],'')
        self.assertEqual(rows[1]['bpm_samples'],0)

    def test_fragments_empty_and_duplicate_rows(self):
        self.assertEqual(len(self.read([HEADER,ROW[:10],ROW[10:],ROW,END])),1)
        self.assertEqual(self.read([HEADER,b'# No saved A sessions\n']),[])

    def test_incomplete_or_wrong_terminator_is_rejected(self):
        for end in ([],[b'# END_HISTORY\n'],[b'DATA,station,status\n']):
            with self.assertRaises(TimeoutError):self.read([HEADER,ROW]+end)
        with self.assertRaises(ValueError):self.read([HEADER,ROW,ROW.replace(b',5,',b',6,'),END])
        with self.assertRaises(ValueError):self.read([b'A_EVENT,part_a,pause_before_history\n'])

    def test_invalid_and_absent_bpm_are_distinct(self):
        for raw in ('ASAVED,1,squats,1,2,,3,1','ASAVED,1,squats,1,2,0,3,1',
                    'ASAVED,1,squats,1,2,nan,3,1','ASAVED,1,squats,1,2,72,0,1',
                    'ASAVED,1,squats,1,32768,,0,1','ASAVED,1,squats,-1,2,,0,1',
                    'ASAVED,1,treadmill,1,2,,0,1','ASAVED,1,squats,1,2,,0,0'):
            with self.subTest(raw=raw),self.assertRaises(ValueError):export.parse_a_saved(raw)

    def test_csv_contains_original_measurements_and_no_hold_columns(self):
        rows=self.read([HEADER,ROW,NO_BPM,END])
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'saved_sessions'/'a.csv';export.write_csv(path,rows,part_a=True)
            with path.open(newline='',encoding='utf-8-sig') as f:
                reader=csv.DictReader(f);saved=list(reader)
                self.assertEqual(reader.fieldnames,export.PART_A_COLUMNS)
            self.assertEqual(saved[1]['bpm_avg'],'');self.assertTrue(saved[0]['exported_at_utc'])
            with self.assertRaises(FileExistsError):export.write_csv(path,rows,part_a=True)

if __name__=='__main__':unittest.main()
