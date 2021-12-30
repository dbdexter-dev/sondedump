#!/usr/bin/env python3

import matplotlib.pyplot as plt
import sys

pre = []
post = []
with open(sys.argv[1], "r") as f:
    for line in f.readlines():
        try:
            newdata = list(map(float, line.split(" ")))
            if (abs(newdata[0]) < 100 and abs(newdata[1]) < 100):
                pre.append(newdata[0])
                post.append(newdata[1])
        except ValueError:
            pass
        except IndexError:
            pass

plt.plot(pre)
plt.plot(post)
plt.show()

