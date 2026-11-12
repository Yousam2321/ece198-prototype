"""Capture one board's CSV output; keeps firmware commands usable.
Install: python -m pip install pyserial
Example: python capture_serial.py --port COM4 --output station.csv
Close the Arduino Serial Monitor before starting this program.
"""
import argparse
import csv
import datetime
import pathlib
import threading
import sys
import serial

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True)
    p.add_argument('--output',required=True,type=pathlib.Path)
    a=p.parse_args()
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with serial.Serial(a.port,115200,timeout=0.5) as dev, a.output.open('x',newline='',encoding='utf-8') as out:
        columns='type,device,uptime_ms,exercise,state,active_ms,reps,source,normalized_signal,hr_bpm,hr_status,baseline_bpm,manual_distance_m,time_mode,distance_cm,reference_cm,band_cm,echo_valid,inside_band,range_ms,sample_gap_ms,note'.split(',')
        wearable='type,device,uptime_ms,exercise,state,active_ms,reps,source,normalized_signal,hr_bpm,hr_status,baseline_bpm,manual_distance_m,time_mode,note'.split(',')
        station='type,device,uptime_ms,exercise,state,active_ms,distance_cm,reference_cm,band_cm,echo_valid,inside_band,range_ms,sample_gap_ms,note'.split(',')
        writer=csv.DictWriter(out,fieldnames=['host_time_utc']+columns)
        writer.writeheader()
        print('Recording. Type commands + Enter; Ctrl+C stops. Output:',a.output)
        def commands():
            for line in sys.stdin:
                try: dev.write((line.rstrip('\r\n')+'\n').encode('ascii'))
                except (serial.SerialException,UnicodeError): break
        threading.Thread(target=commands,daemon=True).start()
        try:
            while True:
                raw=dev.readline().decode('utf-8',errors='replace').strip()
                if not raw: continue
                print(raw)
                if raw.startswith(('DATA,','EVENT,','SUMMARY,')):
                    fields=next(csv.reader([raw]))
                    header=wearable if len(fields)>1 and fields[1]=='wearable' else station
                    if header is station and len(fields)==len(station)-1:
                        header=[c for c in station if c!='sample_gap_ms']
                    if len(fields)!=len(header):
                        print('Skipped incomplete row.'); continue
                    row=dict(zip(header,fields))
                    row['host_time_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
                    writer.writerow(row);out.flush()
        except KeyboardInterrupt: print('\nRecording stopped.')

if __name__=='__main__': main()
