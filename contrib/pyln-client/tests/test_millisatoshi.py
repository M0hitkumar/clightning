from pyln.client import Millibronees


def test_sum_radd():
    result = sum([Millibronees(1), Millibronees(2), Millibronees(3)])
    assert int(result) == 6
