"""Run with: python -m unittest discover -s tests -p test_export_sessions.py"""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import csv

spec = importlib.util.spec_from_file_location("export_sessions", Path(__file__).resolve().parents[1]/"tools/export_sessions.py")
export = importlib.util.module_from_spec(spec)
spec.loader.exec_module(export)

HEADER = b"# SAVED,id,exercise,active_ms,in_band_ms,reference_cm,band_cm\n"
ROW = b"SAVED,4,plank,26384,20000,43.9,10.0\n"
DATA = b"DATA,station,1,plank,IDLE,0,,43.9,10,0,0,0,status\n"

class Port:
    def __init__(self, lines): self.lines=list(lines);self.sent=b"";self.now=0
    def write(self, value): self.sent+=value
    def flush(self): pass
    def readline(self): return self.lines.pop(0) if self.lines else b""
    def clock(self): self.now+=0.05;return self.now

class ExportTests(unittest.TestCase):
    def read(self, lines):
        port=Port(lines);rows=export.read_history(port,timeout=1,clock=port.clock)
        self.assertEqual(port.sent,b"history\n");return rows
    def test_history_and_csv(self):
        rows=self.read([DATA,HEADER,ROW,b"SAVED,5,wall_sit,12000,10000,50.0,10.0\n",DATA])
        self.assertEqual([r['session_id'] for r in rows],[4,5])
        self.assertEqual(rows[0]['active_seconds'],'26.384')
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'results.csv';export.write_csv(path,rows)
            with path.open(encoding='utf-8-sig',newline='') as f: saved=list(csv.DictReader(f))
            self.assertEqual(len(saved),2);self.assertEqual(saved[0]['exercise'],'plank')
            self.assertTrue(saved[0]['exported_at_utc'])
            with self.assertRaises(FileExistsError): export.write_csv(path,rows)
    def test_empty(self): self.assertEqual(self.read([HEADER,b'# No saved sessions\n']),[])
    def test_fragmented(self): self.assertEqual(len(self.read([HEADER,ROW[:10],ROW[10:],DATA])),1)
    def test_incomplete_does_not_export(self):
        with self.assertRaises(TimeoutError): self.read([HEADER,ROW])
    def test_malformed_does_not_export(self):
        with self.assertRaises(ValueError): self.read([HEADER,b'SAVED,4,plank\n',DATA])
    def test_unsupported_firmware(self):
        with self.assertRaises(ValueError): self.read([b'EVENT,station,1,wall_sit,IDLE,command_not_available\n'])
    def test_duplicate_id(self): self.assertEqual(len(self.read([HEADER,ROW,ROW,DATA])),1)

if __name__=='__main__': unittest.main()
