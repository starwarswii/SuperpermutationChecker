#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>
//#include <pthread.h>

//example compile and run:
// mpicc -Wall perm-serial.c && ./a.out 9 tests/valid9.in

//when running on the BG/Q, either uncomment this or compile with -DBGQ
//#define BGQ

#ifdef BGQ
	#include <hwi/include/bqc/A2_inlines.h> //for GetTimeBase() on BG/Q
	#define frequency 1600000000.0 //BG/Q processor frequency
#else
	#define GetTimeBase MPI_Wtime
	#define frequency 1.0 //Wtime already returns seconds, so we divide by 1
#endif

FILE* file;

//length of number in file
int length;

//number of symbols
int N;

char* data; //TODO rename?

//should contain all ones if has all permutations
//is char as will only hold 0 or 1. could probably be unsigned : byte
char* checklist;
int permutationCount;

int factorial(int n) {
	
	int result = 1;
	
	for (int i = 2; i <= n; i++) {
		result *= i;
	}
	
	return result;
}

//converts a character to an int,
//also mapping each one lower
//e.g '1' -> 0, 'a' -> 9
//TODO could update this to the safe/unsafe version
//in general may be able to move many functions into a shared util file
int toInt(char c) {

	//TODO could be sped up slightly
	if (c >= '1' && c <= '9') {
		return c - '1';
	}

	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 9;
	}
	
	return -1;
}

char charTable[15] = {
	'1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

//converts into to char, mapping one higher
char toChar(int i) {
	return charTable[i];
}

//loads a string into an int array
//e.g '1' becomes 0
void loadIntoInt(char* string, int* array) {
	for (int i = 0; i < N; i++) {
		array[i] = toInt(string[i]);
	}
}

//loads an int array into a string
//e.g. 0 becomes '1'
void loadIntoString(int* array, char* string) {
	for (int i = 0; i < N; i++) {
		string[i] = toChar(array[i]);
	}
}

//used in both permutation functions
int* elems;

//used in getPermutation()
int* permuted;

//used in getNumber()
int* pos;

//used in permutationToNumber()
int* tempPerm;

//used in numberToPermutation()
//TODO maybe only initalize some of this stuff when needed
//as it will only be used in an error case
char* outString;

//returns the permutation corresponding
//to the given number k
int* getPermutation(int k) {
	int ind;
	int m = k;
	
	for (int i = 0; i < N; i++) {
		elems[i] = i;
	}
	
	for (int i = 0; i < N; i++) {
		ind = m % (N-i);
		m /= (N-i);
		
		permuted[i] = elems[ind];
		elems[ind] = elems[N-i-1];
	}
	
	return permuted;
}

char* numberToPermutation(int k) {
	int* p = getPermutation(k);
	
	loadIntoString(p, outString);
	
	return outString;
}



//returns the number corresponding to the given permutation
int getNumber(int* perm) {
	
	int k = 0;
	int m = 1;
	
	for (int i = 0; i < N; i++) {
		pos[i] = i;
		elems[i] = i;
	}
	
	for (int i = 0; i < N-1; i++) {
		int posPerm = pos[perm[i]];
		int elemsni1 = elems[N-i-1];
		
		k += m * posPerm;
		m *= (N-i);
		
		pos[elemsni1] = posPerm;
		elems[posPerm] = elemsni1;
	}
	
	return k;
}

//returns the number corresponding to the given permutation string
int permutationToNumber(char* permString) {
	loadIntoInt(permString, tempPerm);
	int k = getNumber(tempPerm);
	
	//getNumber can return invalid permutation numbers
	//if the given string has duplicate numbers
	//in this case we return -1 to indicate it matches no permutation
	if (k >= permutationCount) {
		return -1;
	}
	
	return k;
}

void allocateMemory() {
	
	permutationCount = factorial(N);
	
	data = malloc(length*sizeof(char));
	
	//fill with zeros
	checklist = calloc(permutationCount, sizeof(char));
	
	elems = malloc(N*sizeof(int));
	permuted = malloc(N*sizeof(int));
	pos = malloc(N*sizeof(int));
	
	tempPerm = malloc(N*sizeof(int));
	outString = malloc(N*sizeof(char));
	
}

void freeMemory() {
	free(data);
	free(checklist);
	
	free(elems);
	free(permuted);
	free(pos);
	
	free(tempPerm);
	free(outString);
}

//the main code
void checkNumber() {
	
	long long startTime = GetTimeBase();
	
	for (int i = 0; i < length-N+1; i++) {
		int k = permutationToNumber(data+i);
		
		if (k != -1 && !checklist[k]) {
			checklist[k] = 1;
		}
		
	}
	
	int good = 1;
	printf("checking number...\n");
	for (int i = 0; i < permutationCount; i++) {
		if (!checklist[i]) {
			char* perm = numberToPermutation(i);
			printf("missing permutation number %d: [%.*s]\n", i, N, perm);
			good = 0;
		}
	}
	
	if (good) {
		printf("number is good!\n");
	} else {
		printf("number is not good\n");
	}
	
	long long endTime = GetTimeBase();
	double totalTime = ((double)(endTime-startTime))/frequency;
	printf("time:\n");
	printf("%f\n\n", totalTime);
	
}

int main(int argc, char** argv) {
	
	MPI_Init(&argc, &argv);
	
	int numRanks;
	int rank;
	
	MPI_Comm_size(MPI_COMM_WORLD, &numRanks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	if (numRanks > 1) {
		if (rank == 0) {
			printf("error: this is the serial version, and should only be run with one rank\n");	
		}
		exit(1);
	}

	if (argc < 3) {
		printf("usage: %s N inputFile\n", argv[0]);
		exit(1);
	}

	N = atoi(argv[1]);

	file = fopen(argv[2], "r");
	if (file == NULL) {
		printf("could not open input file %s\n", argv[2]);
		exit(1);
	}

	//we get the length of the file by seeking to the end and
	//then checking where we are
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	
	//then we go back to the start of the file
	fseek(file, 0, SEEK_SET);
	
	printf("running serial version\n");
	
	#ifdef BGQ
		printf("compiled for BG/Q\n");
	#else
		printf("not compiled for BG/Q\n");
	#endif
	
	printf("N = %d\n", N);
	printf("file size detected as %d\n", length);

	allocateMemory();

	//read length bytes into data
	fread(data, sizeof(char), length, file);
	fclose(file);

	checkNumber();

	freeMemory();
	
	MPI_Finalize();
	return 0;
}
