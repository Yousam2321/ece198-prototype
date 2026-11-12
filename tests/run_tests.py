"""Run portable C++ timing/detection tests. Requires g++ on PATH."""
from pathlib import Path
import shutil
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
a=(root/'WearableMonitor/PrototypeCore.h').read_bytes()
b=(root/'HoldStation/PrototypeCore.h').read_bytes()
assert a==b, 'The two PrototypeCore.h copies differ; update both.'
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
print('PASS: shared header copies are identical.')
