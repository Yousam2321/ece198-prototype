"""Profiles receive fresh defaults without overwriting saved participant choices."""
import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'desktop'))
from criteria_defaults import DEFAULT_TARGETS,profile_criteria

class CriteriaDefaultsTests(unittest.TestCase):
    def test_new_empty_and_partial_profiles(self):
        for saved in (None,{},dict(targets={})):
            self.assertEqual(profile_criteria(saved),dict(targets=DEFAULT_TARGETS,minimum=90,rep_minimum=90))
        settings=profile_criteria(dict(targets={'wall_sit':25},minimum=83))
        self.assertEqual(settings['targets']['wall_sit'],25)
        self.assertEqual(settings['targets']['squats'],5)
        self.assertEqual((settings['minimum'],settings['rep_minimum']),(83,90))

    def test_custom_values_are_preserved_and_inputs_are_not_mutated(self):
        saved=dict(targets={key:i+10 for i,key in enumerate(DEFAULT_TARGETS)},minimum=87,rep_minimum=76)
        snapshot=copy.deepcopy(saved)
        self.assertEqual(profile_criteria(saved),snapshot)
        profile_criteria(saved)['targets']['wall_sit']=500
        self.assertEqual(saved,snapshot)
        self.assertEqual(profile_criteria()['targets'],DEFAULT_TARGETS)

    def test_only_empty_values_fall_back_invalid_nonempty_values_stay_for_validation(self):
        result=profile_criteria(dict(targets={'squats':'','plank':None,'wall_sit':0},minimum='',rep_minimum=None))
        self.assertEqual(result['targets']['squats'],5);self.assertEqual(result['targets']['plank'],5)
        self.assertEqual(result['targets']['wall_sit'],0)
        self.assertEqual((result['minimum'],result['rep_minimum']),(90,90))

if __name__=='__main__':unittest.main()
