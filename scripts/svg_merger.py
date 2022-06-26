#!/usr/bin/env python3

import struct

size = 65536
tile_size = 1024
tile_count = size // tile_size

indices = []

fdata = open("tiledata.bin", "wb")
for x in range(tile_count):
    for y in range(tile_count):
        for layer in range(1):

            path = "binary_svgs/{}_{}_{}.bin".format(x, y, layer)

            try:
                with open(path, "rb") as f:
                    indices.append(fdata.tell())
                    fdata.write(f.read())
            except FileNotFoundError:
                indices.append(-1)


fdata.close()

with open("tileindex.bin", "wb") as f:
    f.write(struct.pack('i' * len(indices), *indices))


