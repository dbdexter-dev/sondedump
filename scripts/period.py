#!/usr/bin/env python3

from tqdm import tqdm

def lfsr(state, poly):
    new_bit = 0
    for i in poly[1:]:
        new_bit ^= (state >> i) & 0x1

    state = ((state >> 1) | (new_bit << max(poly)-1))
    return (state, new_bit)


def get_period(poly):
    start_state = (1 << max(poly)) - 1
    (state, _) = lfsr(start_state, poly)
    period = 0
    states = []

    output = []

    while state not in states:
        period += 1
        states.append(state)
        (state, out) = lfsr(state, poly)

        output.append("{}".format(out))

        if state == 0:
            break;

    period = 1 if state == 0 else len(states) - states.index(state)
    return (period, "".join(output))



'''
poly_len = 14
for p in tqdm(range(2,2**poly_len), disable=False):
    poly_str = "{:032b}".format(p)

    poly = []
    for i,digit in enumerate(poly_str):
        if digit == '1':
            poly.append(31-i)

    (period, output) = get_period(poly)
    print("\n{}\t{}\t{}".format(period, poly, output))
'''
poly = [16, 7, 5, 3, 1]
(period, output) = get_period(poly)
print("{}\t{}".format(period, poly))
print("{}".format(output))
