#!/usr/bin/env python3

import matplotlib.pyplot as plt
import sys

normalize = False

data = [[]]
with open(sys.argv[1], "r") as f:
    for line in f.readlines():
        try:
            newdata = list(map(float, line.strip().split(" ")))
            for i,val in enumerate(newdata):
                if i >= len(data):
                    data.append([0 for i in range(len(data[0]) - 1)])
                data[i].append(val)
        except ValueError:
            pass
        except IndexError:
            pass


for sequence in data:
    if normalize:
        (vmin, vmax) = (min(sequence), max(sequence))
        sequence = [(x-vmin)/(vmax-vmin) for x in sequence]
    plt.plot(sequence)
plt.show()

