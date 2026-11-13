"""Editable quick-test defaults, applied without changing saved profile settings."""
DEFAULT_TARGETS={key:5 for key in ('wall_sit','plank','chest_press','squats','lateral_raises')}

def profile_criteria(saved=None):
    saved=saved or {}
    goals=saved.get('targets') or {}
    def present(value):return value is not None and str(value).strip()!=''
    return dict(targets={key:goals[key] if present(goals.get(key)) else value
                         for key,value in DEFAULT_TARGETS.items()},
                minimum=saved['minimum'] if present(saved.get('minimum')) else 90,
                rep_minimum=saved['rep_minimum'] if present(saved.get('rep_minimum')) else 90)
