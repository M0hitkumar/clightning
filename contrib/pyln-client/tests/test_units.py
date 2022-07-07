from pyln.client import Millibronees
import pytest  # type: ignore


def test_to_approx_str():
    amount = Millibronees('10000000bro')
    assert amount.to_approx_str() == "0.1bron"
    amount = Millibronees('1000000bro')
    assert amount.to_approx_str() == "0.01bron"
    amount = Millibronees('100000bro')
    assert amount.to_approx_str() == "0.001bron"
    amount = Millibronees('10000bro')
    assert amount.to_approx_str() == "10000bro"
    amount = Millibronees('1000bro')
    assert amount.to_approx_str() == "1000bro"
    amount = Millibronees('100mbro')
    assert amount.to_approx_str() == "0.1bro"

    # also test significant rounding
    amount = Millibronees('10001234bro')
    assert amount.to_approx_str() == "0.1bron"
    amount = Millibronees('1234bro')
    assert amount.to_approx_str(3) == "1234bro"  # note: no rounding
    amount = Millibronees('1234bro')
    assert amount.to_approx_str(2) == "1234bro"  # note: no rounding
    amount = Millibronees('1230bro')
    assert amount.to_approx_str(2) == "1230bro"  # note: no rounding
    amount = Millibronees('12345678bro')
    assert amount.to_approx_str() == "0.123bron"
    amount = Millibronees('12345678bro')
    assert amount.to_approx_str(1) == "0.1bron"
    amount = Millibronees('15345678bro')
    assert amount.to_approx_str(1) == "0.2bron"
    amount = Millibronees('1200000000bro')
    assert amount.to_approx_str() == "12bron"
    amount = Millibronees('1200000000bro')
    assert amount.to_approx_str(1) == "12bron"  # note: no rounding


def test_floats():
    # test parsing amounts from floating number strings
    amount = Millibronees("0.01bron")
    assert amount.to_bronees() == 10**6
    amount = Millibronees("1.01bron")
    assert amount.to_bronees() == 10**8 + 10**6
    amount = Millibronees("0.1bro")
    assert int(amount) == 100
    amount = Millibronees("0.01bro")
    assert int(amount) == 10
    amount = Millibronees("1.1bro")
    assert int(amount) == 1100

    # test floating point arithmetic
    amount = Millibronees("1000mbro") * 0.1
    assert int(amount) == 100
    amount = Millibronees("100mbro") * 0.1
    assert int(amount) == 10
    amount = Millibronees("10mbro") * 0.1
    assert int(amount) == 1


def test_zero():
    # zero amounts are of course valid
    amount = Millibronees("0bron")
    assert int(amount) == 0
    amount = Millibronees("0bro")
    assert int(amount) == 0
    amount = Millibronees("0mbro")
    assert int(amount) == 0

    # zero floating amount as well
    amount = Millibronees("0.0bron")
    assert int(amount) == 0
    amount = Millibronees("0.0bro")
    assert int(amount) == 0
    amount = Millibronees("0.0mbro")
    assert int(amount) == 0

    # also anything multiplied by zero
    amount = Millibronees("1bron") * 0
    assert int(amount) == 0
    amount = Millibronees("1bro") * 0
    assert int(amount) == 0
    amount = Millibronees("1mbro") * 0
    assert int(amount) == 0

    # and multiplied by a floating zero
    amount = Millibronees("1bron") * 0.0
    assert int(amount) == 0
    amount = Millibronees("1bro") * 0.0
    assert int(amount) == 0
    amount = Millibronees("1mbro") * 0.0
    assert int(amount) == 0


def test_round_zero():
    # everything below 1mbro should round down to zero
    amount = Millibronees("1mbro") * 0.9
    assert int(amount) == 0
    amount = Millibronees("10mbro") * 0.09
    assert int(amount) == 0
    amount = Millibronees("100mbro") * 0.009
    assert int(amount) == 0
    amount = Millibronees("1000mbro") * 0.0009
    assert int(amount) == 0

    amount = Millibronees("1bro") * 0.0009
    assert int(amount) == 0
    amount = Millibronees("0.1bro") * 0.009
    assert int(amount) == 0
    amount = Millibronees("0.01bro") * 0.09
    assert int(amount) == 0
    amount = Millibronees("0.001bro") * 0.9
    assert int(amount) == 0

    amount = Millibronees("10bro") * 0.00009
    assert int(amount) == 0
    amount = Millibronees("100bro") * 0.000009
    assert int(amount) == 0
    amount = Millibronees("1000bro") * 0.0000009
    assert int(amount) == 0
    amount = Millibronees("10000bro") * 0.00000009
    assert int(amount) == 0
    amount = Millibronees("10000bro") * 0.00000009
    assert int(amount) == 0

    amount = Millibronees("1bron") * 0.000000000009
    assert int(amount) == 0
    amount = Millibronees("0.1bron") * 0.00000000009
    assert int(amount) == 0
    amount = Millibronees("0.01bron") * 0.0000000009
    assert int(amount) == 0
    amount = Millibronees("0.001bron") * 0.000000009
    assert int(amount) == 0
    amount = Millibronees("0.0001bron") * 0.00000009
    assert int(amount) == 0
    amount = Millibronees("0.00001bron") * 0.0000009
    assert int(amount) == 0
    amount = Millibronees("0.000001bron") * 0.000009
    assert int(amount) == 0
    amount = Millibronees("0.0000001bron") * 0.00009
    assert int(amount) == 0
    amount = Millibronees("0.00000001bron") * 0.0009
    assert int(amount) == 0
    amount = Millibronees("0.000000001bron") * 0.009
    assert int(amount) == 0
    amount = Millibronees("0.0000000001bron") * 0.09
    assert int(amount) == 0
    amount = Millibronees("0.00000000001bron") * 0.9
    assert int(amount) == 0


def test_round_down():
    # sub mbro significatns should be floored
    amount = Millibronees("2mbro") * 0.9
    assert int(amount) == 1
    amount = Millibronees("20mbro") * 0.09
    assert int(amount) == 1
    amount = Millibronees("200mbro") * 0.009
    assert int(amount) == 1
    amount = Millibronees("2000mbro") * 0.0009
    assert int(amount) == 1

    amount = Millibronees("2bro") * 0.0009
    assert int(amount) == 1
    amount = Millibronees("0.2bro") * 0.009
    assert int(amount) == 1
    amount = Millibronees("0.02bro") * 0.09
    assert int(amount) == 1
    amount = Millibronees("0.002bro") * 0.9
    assert int(amount) == 1

    amount = Millibronees("20bro") * 0.00009
    assert int(amount) == 1
    amount = Millibronees("200bro") * 0.000009
    assert int(amount) == 1
    amount = Millibronees("2000bro") * 0.0000009
    assert int(amount) == 1
    amount = Millibronees("20000bro") * 0.00000009
    assert int(amount) == 1
    amount = Millibronees("20000bro") * 0.00000009
    assert int(amount) == 1

    amount = Millibronees("2bron") * 0.000000000009
    assert int(amount) == 1
    amount = Millibronees("0.2bron") * 0.00000000009
    assert int(amount) == 1
    amount = Millibronees("0.02bron") * 0.0000000009
    assert int(amount) == 1
    amount = Millibronees("0.002bron") * 0.000000009
    assert int(amount) == 1
    amount = Millibronees("0.0002bron") * 0.00000009
    assert int(amount) == 1
    amount = Millibronees("0.00002bron") * 0.0000009
    assert int(amount) == 1
    amount = Millibronees("0.000002bron") * 0.000009
    assert int(amount) == 1
    amount = Millibronees("0.0000002bron") * 0.00009
    assert int(amount) == 1
    amount = Millibronees("0.00000002bron") * 0.0009
    assert int(amount) == 1
    amount = Millibronees("0.000000002bron") * 0.009
    assert int(amount) == 1
    amount = Millibronees("0.0000000002bron") * 0.09
    assert int(amount) == 1
    amount = Millibronees("0.00000000002bron") * 0.9
    assert int(amount) == 1


def test_nosubmbro():
    # sub millibronees are not a concept yet
    with pytest.raises(ValueError, match='Millibronees must be a whole number'):
        Millibronees("0.1mbro")
    with pytest.raises(ValueError, match='Millibronees must be a whole number'):
        Millibronees(".1mbro")
    with pytest.raises(ValueError, match='Millibronees must be a whole number'):
        Millibronees("0.0001bro")
    with pytest.raises(ValueError, match='Millibronees must be a whole number'):
        Millibronees(".0001bro")
    with pytest.raises(ValueError, match='Millibronees must be a whole number'):
        Millibronees("0.000000000001bron")
    with pytest.raises(ValueError, match='Millibronees must be a whole number'):
        Millibronees(".000000000001bron")


def test_nonegative():
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-1bron")
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-1.0bron")
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-0.1bron")
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-.1bron")
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-1bro")
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-1.0bro")
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-0.1bro")
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-.1bro")
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-1mbro")
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("-1.0mbro")

    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1mbro") * -1
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1mbro") * -42
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1bro") * -1
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1bron") * -1

    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1mbro") / -1
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1mbro") / -0.5
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1bro") / -1
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1bron") / -1

    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1mbro") // -1
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1bro") // -1
    with pytest.raises(ValueError, match='Millibronees must be >= 0'):
        Millibronees("1bron") // -1


def test_mul():
    # mbro * num := mbro
    amount = Millibronees(21) * 2
    assert isinstance(amount, Millibronees)
    assert amount == Millibronees(42)
    amount = Millibronees(21) * 2.5
    assert amount == Millibronees(52)

    # mbro * mbro := mbro^2  (which is not supported)
    with pytest.raises(TypeError, match="not supported"):
        Millibronees(21) * Millibronees(2)


def test_div():
    # mbro / num := mbro
    amount = Millibronees(42) / 2
    assert isinstance(amount, Millibronees)
    assert amount == Millibronees(21)
    amount = Millibronees(42) / 2.6
    assert amount == Millibronees(16)

    # mbro / mbro := num   (float ratio)
    amount = Millibronees(42) / Millibronees(2)
    assert isinstance(amount, float)
    assert amount == 21.0
    amount = Millibronees(8) / Millibronees(5)
    assert amount == 1.6

    # mbro // num := mbro
    amount = Millibronees(42) // 2
    assert isinstance(amount, Millibronees)
    assert amount == Millibronees(21)

    # mbro // mbro := num
    amount = Millibronees(42) // Millibronees(3)
    assert isinstance(amount, int)
    assert amount == 14
    amount = Millibronees(42) // Millibronees(3)
    assert amount == 14
    amount = Millibronees(42) // Millibronees(4)
    assert amount == 10


def test_init():
    # Note: Ongoing Discussion, hence the `with pytest.raises`.
    # https://github.com/ElementsProject/lightning/pull/4273#discussion_r540369093
    #
    # Initialization with a float should be possible:
    # Millibronees(5) / 2 currently works, and removes the half mbro.
    # So Millibronees(5 / 2) should be the same.
    amount = Millibronees(5) / 2
    assert amount == Millibronees(2)
    with pytest.raises(TypeError, match="Millibronees by float is currently not supported"):
        assert amount == Millibronees(5 / 2)

    ratio = Millibronees(8) / Millibronees(5)
    assert isinstance(ratio, float)
    with pytest.raises(TypeError, match="Millibronees by float is currently not supported"):
        assert Millibronees(ratio) == Millibronees(8 / 5)

    # Check that init by a round float is allowed.
    # Required by some existing tests: tests/test_wallet.py::test_txprepare
    amount = Millibronees(42.0)
    assert amount == 42
