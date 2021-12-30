#!/usr/bin/env python3

import matplotlib.pyplot as plt
import sys
import math

normalize = True
remove_outliers = True
show_3d = False

points = []
with open(sys.argv[1], "rb") as f:
    for line in f.readlines():
        numbers = list(filter(len, line.split(b",")))
        for i in range(len(numbers)):
            try:
                numbers[i] = float(numbers[i])
            except ValueError:
                numbers[i] = 0

        try:
            points.append(numbers)
        except IndexError:
            pass

ebs_vel = list(map(lambda x: math.sqrt(x[0]**2+x[1]**2), zip([x[1] for x in points], [x[2] for x in points])))
for i in range(len(points)):
    points[i].append(abs_vel[i])

for i in range(len(points[0])):
    ys = [x[i] for x in points]

    if remove_outliers:
        mu = sum(ys)/len(ys)
        sigma = math.sqrt(sum(map(lambda x: x**2, ys))/len(ys))

        print("{} {}".format(mu, sigma))

        ys = [y if abs((y-mu)/sigma) < 1 else mu for y in ys]


    if normalize:
        miny = min(filter(lambda x: x == x, ys))
        maxy = max(filter(lambda x: x == x,ys))
        print("[{}..{}]".format(miny, maxy))
        if miny != maxy:
            ys = [(x-miny)/(maxy-miny) for x in ys]

    plt.plot(ys)
plt.legend(list(range(len(points[0]))))
plt.show()
