"""Read both independent archives without reconnecting/resetting between them."""
from export_sessions import read_history, read_rep_history
from evaluation import normalize


def read_all_history(device, *, symptom_snapshot=None, **kwargs):
    metadata={}
    holds=read_history(device,metadata=metadata,**kwargs)
    if symptom_snapshot is not None and "symptoms" in metadata:
        generation,counts=metadata["symptoms"]
        symptom_snapshot.update(generation=generation,counts=counts)
    if metadata.get('device')=='part_a':return [normalize(r) for r in holds], '' 
    try:reps=read_rep_history(device,**kwargs)
    except ValueError as error:
        # B2.4 and earlier do not implement rep_history; retain their valid holds.
        if 'Upload B2.5 or newer' not in str(error):raise
        return [normalize(r) for r in holds], 'Older firmware: hold sessions only.'
    return [normalize(r) for r in holds+reps], ''
