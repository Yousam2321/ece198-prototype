"""Short, shared explanations for the desktop view and printable report."""

def result_label(result, minimum):
    if result.get('missing_target'):return 'Not scored - set target'
    if result.get('kind')=='counts':
        return 'Good - rep target met' if result['status']=='Good' else 'Not good yet - rep count'
    if result.get('kind')=='reps':
        if result['status']=='Cannot evaluate':return 'Not scored - no completed attempts'
        if result['status']=='Good':return 'Good - both goals met'
        missing=[]
        if result['remaining_reps']>0:missing.append('rep count')
        if result['good_rep_percent']<result['minimum_percent']:missing.append('good-rep %')
        return 'Not good yet - '+' + '.join(missing)
    if result['status'] == 'Cannot evaluate':
        return 'Not scored - missing ending data' if result['trailing_seconds'] is None else 'Not scored - no valid hold'
    if result['status'] == 'Good':
        return 'Good - both goals met'
    missing=[]
    if result['remaining_seconds'] > 0: missing.append('time')
    if result['consistency_percent'] < minimum: missing.append('consistency')
    return 'Not good yet - ' + ' + '.join(missing)


def short_feedback(result, minimum):
    if result.get('missing_target'):return result['reasons'][0]
    if result.get('kind')=='counts':
        return 'Recorded rep target met.' if result['status']=='Good' else f"Needs {result['remaining_reps']:g} more reps."
    if result.get('kind')=='reps':
        if result['status']=='Cannot evaluate':return result['reasons'][0]
        needs=[]
        if result['remaining_reps']>0:needs.append(f"{result['remaining_reps']:g} more good rep"+("s" if result['remaining_reps']!=1 else ""))
        if result['good_rep_percent']<result['minimum_percent']:
            needs.append(f"{result['minimum_percent']-result['good_rep_percent']:.1f} percentage points more good reps")
        return 'Needs '+' and '.join(needs)+'.' if needs else 'Your recorded reps met both confirmed goals.' 
    if result['status'] == 'Cannot evaluate':
        if result['trailing_seconds'] is None:
            return 'This older record lacks ending data. Repeat with the current firmware to score consistency.'
        return 'No valid hold was recorded. Check calibration and sensor placement, then repeat.'
    needs=[]
    if result['remaining_seconds'] > 0:
        needs.append(f"{result['remaining_seconds']:.1f}s more in the target zone")
    if result['consistency_percent'] < minimum:
        needs.append(f"{minimum-result['consistency_percent']:.1f} percentage points more consistency")
    return 'Needs ' + ' and '.join(needs) + '.' if needs else 'Your recorded hold met both confirmed goals.'


def progress_summary(rows,index,change,first=False):
    if not change:return 'No earlier comparable scored session yet.'
    exercise=rows[index]['exercise']
    if change.get('kind')=='counts':
        prefix='first_' if first else ''
        return (f"Since {'first' if first else 'previous'} session of this exercise: reps {change[prefix+'rep_change']:+g}; "
                f"active time {change[prefix+'active_change']:+.1f}s.")
    if change.get('kind')=='reps':
        prefix='first_' if first else ''
        text=(f"Since {'first' if first else 'previous'} chest-press session: good reps {change[prefix+'good_change']:+g}; "
              f"good-rep percentage {change[prefix+'percent_change']:+.1f} percentage points.")
        if change['first_range_changed' if first else 'range_changed']:
            text+=' Calibrated travel changed; the movement distances differ.'
        return text
    prefix='first_' if first else ''
    return (f"Since {'first' if first else 'previous'} {'wall-sit' if exercise=='wall_sit' else 'plank'} session: "
            f"held {change[prefix+'hold_change']:+.1f}s; consistency {change[prefix+'consistency_change']:+.1f} percentage points.")

