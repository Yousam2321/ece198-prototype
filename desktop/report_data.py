"""Shared, testable presentation/export values for hold and repetition sessions."""
import csv
from pathlib import Path
from evaluation import EXERCISES, evaluate


def preview_values(row):
    if row['exercise']=='chest_press':
        return (row['session_id'],EXERCISES[row['exercise']],f"{row['active_ms']/1000:.1f}",
                f"{row['good_reps']} / {row['attempts']} reps",f"{row['attempts']-row['good_reps']} short",f"{abs(row['pressed_cm']-row['start_cm']):.1f}cm travel")
    tail='Unknown' if row['trailing_out_ms'] is None else f"{row['trailing_out_ms']/1000:.1f}s ending"
    return (row['session_id'],EXERCISES[row['exercise']],f"{row['active_ms']/1000:.1f}",f"{row['in_band_ms']/1000:.1f}s in zone",tail,f"±{row['band_cm']:g}cm zone")


def export_assessment_csv(path,participant,rows,targets,minimum,rep_minimum=90):
    raw=['session_id','history_id','exercise','active_ms','in_band_ms','reference_cm','band_cm','trailing_out_ms','has_hold',
         'good_reps','attempts','start_cm','pressed_cm','last_travel_cm']
    derived=['status','active_seconds','in_band_seconds','trailing_seconds','adjusted_active_seconds','uncredited_seconds',
             'consistency_percent','target_seconds','completion_percent','short_attempts','good_rep_percent','target_reps',
             'calibrated_travel_cm','required_travel_cm','return_tolerance_cm']
    columns=['participant']+raw+derived+['required_consistency_percent','required_good_rep_percent','feedback']
    with Path(path).open('x',encoding='utf-8-sig',newline='') as out:
        writer=csv.DictWriter(out,fieldnames=columns);writer.writeheader()
        for row in rows:
            r=evaluate(row,targets,minimum,rep_minimum)
            item={key:row.get(key,'') for key in raw};item.update({key:r.get(key,'') for key in derived})
            item.update(participant=participant,required_consistency_percent=minimum if row['exercise']!='chest_press' else '',
                        required_good_rep_percent=rep_minimum if row['exercise']=='chest_press' else '',feedback=' '.join(r['reasons']))
            writer.writerow(item)
