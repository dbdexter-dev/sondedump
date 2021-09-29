#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np

normalize = True

ref = []
ref_alt = []
measured = []
measured_alt = []
with open("/tmp/known.data", "rb") as f:
    i = 0
    for line in f.readlines():
        numbers = list(map(float, filter(len, line.split(b" "))))
        i += 1
        try:
            ref.append(numbers[4])
            ref_alt.append(numbers[1])
        except IndexError:
            pass
print(ref)
print(ref_alt)

with open("/tmp/plot.data", "rb") as f:
    i = 0
    for line in f.readlines():
        numbers = list(map(float, line.split(b" ")))
        i += 1
        try:
            if all(n > 0 for n in numbers):
                measured.append(numbers[0])
                measured_alt.append(numbers[3])
        except IndexError:
            pass

ref = np.interp(measured_alt, ref_alt, ref)


if normalize:
    low = min(ref)
    high = max(ref)
    ref = [(a-low)/(high-low) for a in ref]
    low = min(measured)
    high = max(measured)
    measured = [(a-low)/(high-low) for a in measured]

plt.plot(ref)
plt.plot(measured)
plt.legend(["Reference", "Measured"])
plt.show()
