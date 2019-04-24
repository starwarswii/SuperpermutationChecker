serial:
	mpixlc -O5 -DBGQ perm-serial.c -o perm-serial.xl
	
parallel:
	mpixlc -O5 -DBGQ perm-parallel.c -o perm-parallel.xl

parallel-unsafe:
	mpixlc -O5 -DBGQ -DUNSAFE perm-parallel.c -o perm-parallel-unsafe.xl
