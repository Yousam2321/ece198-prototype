"""Run portable C++ timing/detection tests. Requires g++ on PATH."""
from pathlib import Path
import shutil
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
compiler=shutil.which('g++')
if not compiler: raise SystemExit('Install g++ or use the Arduino board builds; g++ was not found.')
with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'core_tests'
    if __import__('os').name=='nt': exe=exe.with_suffix('.exe')
    subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-pedantic',str(root/'tests/core_tests.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'station_tests'
    if __import__('os').name=='nt': exe=exe.with_suffix('.exe')
    subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-pedantic','-I',str(root/'tests/stubs'),str(root/'tests/station_tests.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-pedantic','-DENABLE_OLED=1','-I',str(root/'tests/stubs'),str(root/'tests/station_tests.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: Part B controller regressions.')

with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'chest_press_tests'
    if __import__('os').name=='nt': exe=exe.with_suffix('.exe')
    subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-pedantic',str(root/'tests/chest_press_tests.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'parta_tests'
    if __import__('os').name=='nt': exe=exe.with_suffix('.exe')
    subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-pedantic','-include',str(root/'tests/parta_stubs/Arduino.h'),'-I',str(root/'tests/parta_stubs'),str(root/'tests/parta_tests.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    # The R4 uses a separate native USB output backend. Compile the actual
    # sketch against its API too; the fake core writer rejects every call.
    subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-pedantic','-DARDUINO_ARCH_RENESAS_UNO','-include',str(root/'tests/parta_stubs/Arduino.h'),'-I',str(root/'tests/parta_stubs'),str(root/'tests/parta_tests.cpp'),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe),'--emit-history'],check=True,text=True,capture_output=True)
    print(result.stdout.splitlines()[0])
    # Exercise the real producer -> CSV exporter -> desktop import path.
    import sys
    sys.path[:0]=[str(root/'tools'),str(root/'desktop')]
    from export_sessions import parse_a_saved, write_csv
    from evaluation import load_csv
    rows=[parse_a_saved(line) for line in result.stdout.splitlines() if line.startswith('ASAVED,')]
    assert len(rows)>=7 and '# END_A_HISTORY' in result.stdout
    csv_path=Path(d)/'actual_part_a.csv'
    write_csv(csv_path,rows,part_a=True)
    imported=load_csv(csv_path)
    assert len(imported)==len(rows) and all(row['exercise']=='lateral_raises' for row in imported)
    print('PASS: actual A2 streamed history -> saved CSV -> results app import.')
