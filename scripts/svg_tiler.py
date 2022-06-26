#!/usr/bin/env python3
from xml.dom import minidom
from sys import argv
#import matplotlib.pyplot as plt
import math
from functools import reduce
from tqdm import tqdm
import re
import struct

'''
USAGE:

    - Create a SVG with a power-of-two size, set it as the $size parameter below
    - Change tile size based on your preference
    - Make a directory "binary_svgs/"
    - Let it rip: ./svg_tiler.py <svg>

This will generate a bunch of binary files encoding each tile as a vertex buffer {float lat, float lon} followed by an index buffer {uint16_t idx},
which can be fed directly to OpenGL for rendering.
lat/lon are absolute, idx is relative to the vertices in the tile
one file is created for each lat/lon tile, for each color within the tile

'''

size = 65536
tile_size = 1024
PRIMITIVE_RESTART = 0xFFFF

class Tile:
    def __init__(self, start, size):
        self.start = start
        self.end = tuple(map(sum, zip(start, size)))

    def crop(self, points):
        start = points[0]
        end = points[1]

        return (start, end)

    def crosses(self, line):
        start = line[0]
        end = line[1]

        # If one extremity is inside the frame, return true
        if start[0] >= self.start[0] and start[0] <= self.end[0] and start[1] >= self.start[1] and start[1] <= self.end[1]:
            return True
        if end[0] >= self.start[0] and end[0] <= self.end[0] and end[1] >= self.start[1] and end[1] <= self.end[1]:
            return True


        # Handle m = infinity
        if end[0] == start[0]:
            return start[0] >= self.start[0] and start[0] <= self.end[0] and not ((start[1] < self.start[1] and end[1] < self.start[1]) or (start[1] > self.end[1] and end[1] > self.end[1]))

        crosses_x_start = False
        crosses_x_end = False
        crosses_y_start = False
        crosses_y_end = False

        # Compute line equation
        m = (end[1] - start[1]) / (end[0] - start[0])
        q = start[1] - m * start[0]

        line_y = lambda x: m*x + q
        line_x = lambda y: (y - q) / m

        # Check solutions for constant-y delimiters are within y bounds
        crossing_0 = line_y(self.start[0])
        crossing_1 = line_y(self.end[0])

        if crossing_0 > start[1] and crossing_0 < end[1]:
            crosses_x_start = line_y(self.start[0]) > self.start[1] and line_y(self.start[0]) < self.end[1]

        if crossing_1 > start[1] and crossing_1 < end[1]:
            crosses_x_end = line_y(self.end[0]) > self.start[1] and line_y(self.end[0]) < self.end[1]

        # Check solutions for constant-x delimiters are within x bounds
        if m != 0:
            crossing_0 = line_x(self.start[1])
            crossing_1 = line_x(self.end[1])
        else:
            crossing_0 = self.start[0]
            crossing_1 = self.end[0]

        if crossing_0 > start[0] and crossing_0 < end[0]:
            crosses_y_start = crossing_0 > self.start[0] and crossing_0 < self.end[0]

        if crossing_1 > start[0] and crossing_1 < end[0]:
            crosses_y_end = crossing_1 > self.start[0] and crossing_1 < self.end[0]

        return crosses_x_start or crosses_x_end or crosses_y_start or crosses_y_end




class Path:
    def __init__(self, points, color):
        self.points = points
        self.color = color

    def from_string(data):
        m = re.search("stroke:rgb\(([^%]*)%,([^%]*)%,([^%]*)%\)", data[1])
        color = (float(m.group(1))/100.0, float(m.group(2))/100.0, float(m.group(3))/100.0)


        line = data[0]
        metapoints = []
        tmp = None

        elements = line.split(" ")
        for i,element in enumerate(elements):
            if element == 'M':
                tmp = tuple(float(x) for x in elements[i+1:i+3])
                metapoints.append([tmp])
            if element == 'L':
                metapoints[-1].append(tuple(float(x) for x in elements[i+1:i+3]))
            elif element == 'Z':
                metapoints[-1].append(metapoints[-1][0])

        return list(map(lambda x: Path(x, color), metapoints))


    def tile(self, tile):
        tiled_points = [[]]

        prev_point = self.points[0]

        for point in self.points[1:]:
            if tile.crosses([prev_point, point]):
                # Path is visible
                (start, end) = tile.crop((prev_point, point))

                tiled_points[-1].append(start)
                tiled_points[-1].append(end)

            elif len(tiled_points[-1]) > 0:
                tiled_points.append([])


            # Update prev point
            prev_point = point


        return None if len(tiled_points) == 0 else [Path(x, self.color) for x in tiled_points if len(x) > 0]



doc = minidom.parse(argv[1])
path_strings = [(path.getAttribute('d'), path.getAttribute('style')) for path in doc.getElementsByTagName('path')]
doc.unlink()

paths = [Path.from_string(x) for x in path_strings]
paths = reduce(lambda arr,x: arr+x, paths)

tile_count = size // tile_size

#tiles = [Tile((x, y), (256, 256)) for x in range(size//256) for y in range(size//256)]

# plt.ion()
# fig = plt.figure()
# ax = fig.add_subplot(111)
# ax.xlim((0, 20000))
# ax.ylim((0, 20000))

colors = set(map(lambda x: x.color, paths))

for x in tqdm(range(0, size, tile_size), leave=False):
    for y in tqdm(range(0, size, tile_size), position=1, leave=False):
        tile = Tile((x, y), (tile_size, tile_size))
        tilepaths = reduce(lambda arr,x : arr+x, filter(lambda x: x!=None, [x.tile(tile) for x in paths]), [])

        '''
        if len(tilepaths) > 0:
            for path in tilepaths:
                if path != None:
                    xs = [x[0] for x in path.points]
                    ys = [x[1] for x in path.points]
                    plt.plot(xs, ys)
            plt.show()
        '''

        # Optimize vertex array buffer
        for color_idx,color in enumerate(colors):
            color_paths = list(filter(lambda x: x.color == color, tilepaths))

            verts = []
            idxs = []
            for path in tqdm(color_paths, position=2, leave=False):
                for point in path.points:
                    try:
                        i = verts.index(point)
                        idxs.append(i)
                    except ValueError:
                        verts.append(point)
                        idxs.append(len(verts) - 1)


            # Dedupe segments
            for i in range(0, len(idxs), 2):
                for j in range(0, i, 2):
                    if idxs[i] == idxs[j] and idxs[i+1] == idxs[j+1]:
                        idxs[i] = -1;
                        idxs[i+1] = -1;
                    elif idxs[i] == idxs[j+1] and idxs[i+1] == idxs[j]:
                        idxs[i] = -1;
                        idxs[i+1] = -1;
            idxs = [x for x in idxs if x != -1]

            # Convert to line segments split up with restart primitive markers
            if len(idxs) > 0:
                idxs_chain = [PRIMITIVE_RESTART, idxs[0], idxs[1]]
                for i in range(2, len(idxs), 2):
                    if idxs[i] != idxs_chain[-1]:
                        idxs_chain.append(PRIMITIVE_RESTART)
                        idxs_chain.append(idxs[i])
                    idxs_chain.append(idxs[i+1])


                idxs = idxs_chain

            if len(idxs) > 0:
                with open("binary_svgs/{}_{}_{}.bin".format(x//tile_size, y//tile_size, color_idx), 'wb') as f:
                    flattened_verts = reduce(lambda arr,x:arr+x, verts)

                    # Normalize to tile coordinates
                    flattened_verts = list(map(lambda x: x * tile_count/size, flattened_verts))

                    f.write(len(verts).to_bytes(4, 'little'))
                    f.write(struct.pack('f'*len(flattened_verts), *flattened_verts))
                    f.write(len(idxs).to_bytes(4, 'little'))
                    f.write(struct.pack('H' * len(idxs), *idxs))

            '''

            with open("binary_svgs/{}_{}_{}.bin".format(x//tile_size, y//tile_size, color_idx), 'wb+') as f:
                points = list(map(lambda x: x.points, color_paths))
                if (len(points)) > 2:
                    verts = reduce(lambda arr,x:arr + (2*x)[1:-1], points)
                    flattened_verts = reduce(lambda arr,x:arr+x, verts)

                    f.write(len(verts).to_bytes(4, 'little'))
                    f.write(struct.pack('f'*len(flattened_verts), *flattened_verts))
            '''







