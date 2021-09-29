#!/usr/bin/env python3

import matplotlib.pyplot as plt

normalize = False
show_3d = False

points = []
with open("/tmp/plot.data", "rb") as f:
    for line in f.readlines():
        numbers = list(filter(len, line.split(b" ")))
        for i in range(len(numbers)):
            try:
                numbers[i] = float(numbers[i])
            except ValueError:
                numbers[i] = 0

        try:
            if numbers[0] > 0:
                points.append(numbers)
        except IndexError:
            pass

for i in range(1, len(points[0])):
    ys = [x[i] for x in points]

    if normalize:
        miny = min(filter(lambda x: x == x, ys))
        maxy = max(filter(lambda x: x == x,ys))
        print("[{}..{}]".format(miny, maxy))
        if miny - maxy != 0:
            ys = [(x-miny)/(maxy-miny) for x in ys]

    plt.plot([x[0] for x in points], ys)
plt.show()
