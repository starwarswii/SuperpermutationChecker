"Generate a short Superpermutation of n characters A... as a string using various algorithms."
 
 
from __future__ import print_function, division
 
from itertools import permutations
from math import factorial
import string
import datetime
import gc
 
 
universe = ['1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f']
n = 6

allchars = universe[:n]
allperms = [''.join(p) for p in permutations(allchars)]

with open("temp2.txt", "w+") as f:
    print("printing")
    for a in allperms:
        f.write(a)
    print("done")