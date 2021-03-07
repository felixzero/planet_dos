#!/usr/bin/python2

from math import *
import sys
import struct

size_3d = 128
projection_x = 128
projection_y = projection_x/2

projection_lookup_table = [projection_x*projection_y] * size_3d * size_3d

R = size_3d / 2.0

if __name__ == "__main__":
    for x in range(size_3d):
        for y in range(size_3d):
            latitude = asin((y-64)/R) # in [-pi/2, pi/2]
            sin_longitude = (x-64)/R/cos(latitude)
            if abs(sin_longitude) > 1:
                continue
            longitude = asin(sin_longitude) # in [-pi/2, pi/2]
            i = int((longitude+pi/2)/pi*projection_x)/2
            j = int((sin(latitude) + 1)/2.0*projection_y)
            projection_lookup_table[y*size_3d + x] = i*projection_y + j;
    
    if len(sys.argv) != 2:
        print "Usage: project.py outfile"
        exit(1)
    
    f = open(sys.argv[1], "wb")
    for x in projection_lookup_table:
        f.write(struct.pack("<H", x))
    f.close()
    

