#!/usr/bin/env python3

import matplotlib.pyplot as plt
import sys

pre = []
post = []
with open(sys.argv[1], "r") as f:
    for line in f.readlines():
        newdata = list(map(float, line.split(" ")))
        pre.append(newdata[0])
        post.append(newdata[1])

plt.plot(pre)
plt.plot(post)
plt.show()

