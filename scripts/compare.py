#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np
import re

sondy_data = []
'''
with open("/tmp/S2641451.csv", "rb") as f:
    for line in f.readlines():
        data = line.split(b";");
        try:
            altitude = float(data[7]);
            infostr = data[-1]

            rh = re.search(r"h=[0-9]+\.?[0-9]?%", str(infostr))
            if rh:
                rh = float(rh.group()[2:-1])
            sondy_data.append((altitude, rh))
        except IndexError:
            pass
        except ValueError:
            pass

'''
files = ["/tmp/known_27.data", "/tmp/27.data"]
#cal = [44.522408, 5.025059] # 23
#cal = [45.366291, 4.996685] # 24
#cal = [44.669968, 4.999325] # 25
#cal = [43.727192, 4.978656] # 27


def rmse(x, target):
    return np.sqrt(np.mean((np.array(x) - np.array(target))**2))

def eval_poly(poly, x):
    result = 0
    for coeff in poly:
        result = result*x + coeff
    return result

# Load reference
ref_data = []
with open(files[0], "rb") as f:
    for line in f.readlines():
        data = list(filter(len, line.split(b" ")))
        altitude = float(data[1])
        rh = float(data[4])

        ref_data.append((altitude, rh))


# Load raw
raw_data = []
temp_data = []
old_rh = 0
#cal = [45.366291, 4.996685] # 24
with open(files[1], "rb") as f:
    maxalt = 0
    for line in f.readlines():
        data = list(filter(len, line.split(b" ")))
        if len(data) > 1:
            altitude = float(data[0])
            temp = float(data[1])
            rh = float(data[2])
            rh = max(0, min(100, rh))
            '''
            rh_meas = (adc - adc_ref1) / (adc_ref2 - adc_ref1)
            rh = 100 * (350/cal[0]*rh_meas - 7.5)
            rh += -temp/5.5
            if (temp < -25):
                rh *= 1 -25/90
            '''
            '''
            rh_meas = (adc - adc_ref1) / (adc_ref2 - adc_ref1)
            rh = (rh_meas - 1) * 100 + 5
            d1u = cal[0]

            if temp < -15:
                d2u = 0.005*temp**2 + 0.112*temp + 0.404
            else:
                d2u = 0

            rh = rh + d1u*rh/100 + d2u*rh/100
            rh *= cal[1]
            '''

            raw_data.append((altitude, rh))
            old_rh = rh

raw_data = list(sorted(raw_data, key=lambda x: x[0]))
print(min([x[1] for x in raw_data]))
print((max([x[1] for x in raw_data])-min([x[1] for x in raw_data])))


plt.plot([x[0] for x in ref_data], [x[1] for x in ref_data], marker="o" )
plt.plot([x[0] for x in raw_data], [x[1] for x in raw_data])
plt.plot([x[0] for x in temp_data], [x[1] for x in temp_data])
plt.plot([x[0] for x in sondy_data], [x[1] for x in sondy_data])
plt.legend(["Reference", "Measured", "Temp", "Sondy"])
plt.show()

ref_data = list(zip([x[0] for x in raw_data], np.interp([x[0] for x in raw_data], [x[0] for x in ref_data], [x[1] for x in ref_data])))

print("RMSE: {}".format(rmse([x[1] for x in raw_data], [x[1] for x in ref_data])))

'''
error = [raw_data[i][1] - ref_data[i][1] for i in range(len(ref_data))]
poly = np.polyfit([x[1] for x in raw_data], error, 2)

print(poly)
poly_curve = list(map(lambda x: eval_poly(poly, x), [x[1] for x in raw_data]))
plt.plot([x[1] for x in raw_data], error)
plt.plot([x[1] for x in raw_data], poly_curve)
plt.show()
'''
