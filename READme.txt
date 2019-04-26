Parallel Programing

This program is to check superpermutations for validity in a parallel way. Generating superpermutations is an NP-complete problem. We incldued a python program to generate superpermutations with different number of symbols. 

Usage:

To compile:
    mpicc perm.c -o perm.out
To run the program:
    mpirun -np [number of ranks] ./perm.out [number of symbols] [test file]
