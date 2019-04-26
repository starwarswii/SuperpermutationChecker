#standard compilation
serial:
	mpicc -Wall perm-serial.c

parallel:
	mpicc -Wall perm-parallel.c

hyperparallel:
	mpicc -Wall perm-hyperparallel.c -lpthread

#compilation for the BG/Q
bgq-serial:
	mpixlc -O5 -DBGQ perm-serial.c -o perm-serial.xl

bgq-parallel:
	mpixlc -O5 -DBGQ perm-parallel.c -o perm-parallel.xl

bgq-hyperparallel:
	mpixlc -O5 -DBGQ perm-hyperparallel.c -lpthread -o perm-hyperparallel.xl

#"unsafe" compilations. may be slightly faster
#have undefined behavior on malformed input
parallel-unsafe:
	mpicc -Wall -DUNSAFE perm-parallel.c
hyperparallel-unsafe:
	mpicc -Wall -DUNSAFE perm-hyperparallel.c -lpthread

bgq-hyperparallel-unsafe:
	mpixlc -O5 -DBGQ -DUNSAFE perm-hyperparallel.c -lpthread -o perm-hyperparallel-unsafe.xl

bgq-parallel-unsafe:
	mpixlc -O5 -DBGQ -DUNSAFE perm-parallel.c -o perm-parallel-unsafe.xl
