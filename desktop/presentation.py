"""Short, shared explanations for the desktop view and printable report."""

def result_label(result, minimum):
    if result['status'] == 'Cannot evaluate':
        return 'Not scored - missing ending data' if result['trailing_seconds'] is None else 'Not scored - no valid hold'
    if result['status'] == 'Good':
        return 'Good - both goals met'
    missing=[]
    if result['remaining_seconds'] > 0: missing.append('time')
    if result['consistency_percent'] < minimum: missing.append('consistency')
    return 'Not good yet - ' + ' + '.join(missing)


def short_feedback(result, minimum):
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
