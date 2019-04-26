# Superpermutation Checker
Checks superpermutations for validity

includes serial, parallel, and parallel+threaded versions

we have also included some tests, utilities for generating tests, and a testing program

## our programs:
perm-serial.c
perm-parallel.c
perm-hyperparallel.c

these can be compiled using the provided Makefile, which provides options for normal and BG/Q builds

they are run with ./a.out N inputFile [threadsPerRank]
where N is the size of the input alphebet, and inputFile is the filename with the string to check
the filename should only contain the ascii characters '1', '2', ..., to N, using 'a', 'b', etc if N is larger than 9
the file should also not have a trailing newline. that is to say, the size of the file in bytes should be identical to the number of characters in the string
threadsPerRank is only used in perm-hyperparallel.c, and specifies how many threads to spawn in each rank.
specifiying, e.g. 4, will spawn 3 pthreads in each rank to help the mpi rank do the computation

## our tools:
finderrors.py
generate.py

finderrors will compile and run our progams with various configurations, checking for correct output
generate.py will generate very long unoptimized superpermutations for stress-test inputs

## other tools (not by us):
SuperPermutations.c

SuperPermutations.c is written by Greg Egan and is taken directly from his site [here](https://www.gregegan.net/SCIENCE/Superpermutations/Superpermutations.html).
it can generate more optimal superpermutations for testing input
