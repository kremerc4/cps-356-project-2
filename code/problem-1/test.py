"""Test meal counts, buffer limits, and input errors."""
from collections import deque
from itertools import product
from pathlib import Path
import re
import subprocess
import sys

EVENT = re.compile(
    r"(Chef|Waiter) (\d+) (prepared|served) meal (\d+) "
    r"\(buffer size = (\d+)\)"
)


def check(executable, chefs, waiters, capacity, meals):
    result = subprocess.run(
        [executable], input=f"{chefs} {waiters} {capacity} {meals}\n",
        text=True, capture_output=True, timeout=10, check=True,
    )
    queue = deque()
    produced, served = [], []
    per_chef = [0] * chefs
    peak = 0
    summary = {}
    for line in result.stdout.splitlines():
        match = EVENT.fullmatch(line)
        if match:
            role, worker, action, meal, size = match.groups()
            worker, meal, size = map(int, (worker, meal, size))
            if role == "Chef":
                assert action == "prepared" and 1 <= worker <= chefs
                assert len(queue) < capacity, "Buffer overflow"
                queue.append(meal)
                produced.append(meal)
                per_chef[worker - 1] += 1
            else:
                assert action == "served" and 1 <= worker <= waiters
                assert queue, "Buffer underflow"
                assert queue.popleft() == meal, "FIFO order violated"
                served.append(meal)
            assert size == len(queue) and 0 <= size <= capacity
            peak = max(peak, size)
        elif ": " in line:
            key, value = line.split(": ")
            summary[key] = int(value)
        else:
            assert line in ("", "Execution Summary"), line
    expected = chefs * meals
    assert not queue
    assert produced == list(range(1, expected + 1)), "Missing/duplicate meal IDs"
    assert served == produced, "Not every meal was served exactly once"
    assert per_chef == [meals] * chefs
    assert summary == {
        "Total meals prepared": expected,
        "Total meals served": expected,
        "Maximum buffer usage": peak,
    }
    assert not result.stderr


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python code/problem-1/test.py PATH_TO_EXECUTABLE")
    executable = str(Path(sys.argv[1]).resolve(strict=True))
    cases = list(product(range(1, 6), range(1, 6), (5, 6, 20), (1, 3, 20)))
    cases += [(2, 2, 5, 3)] * 50  # Repeat the sample with different thread schedules.
    for case in cases:
        check(executable, *case)
    invalid = (
        "", "1 1 5", "0 1 5 1", "6 1 5 1", "1 0 5 1", "1 6 5 1",
        "1 1 4 1", "1 1 21 1", "1 1 5 0", "1 1 5 21", "x 1 5 1",
        "1.5 1 5 1", "99999999999999999999999999999999999999 1 5 1",
    )
    for value in invalid:
        result = subprocess.run(
            [executable], input=value, text=True, capture_output=True, timeout=10
        )
        assert result.returncode != 0 and result.stderr and not result.stdout
    print(f"PASS: {len(cases)} valid runs and {len(invalid)} invalid inputs")


if __name__ == "__main__":
    main()
