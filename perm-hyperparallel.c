#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>
#include <pthread.h>

//example compile and run:
// mpicc -Wall perm-hyperparallel.c -lpthread && mpirun -n 2 ./a.out 9 tests/valid9.in 2

//when running on the BG/Q, either uncomment this or compile with -DBGQ
//#define BGQ

#ifdef BGQ
	#include <hwi/include/bqc/A2_inlines.h> //for GetTimeBase() on BG/Q
	#define frequency 1600000000.0 //BG/Q processor frequency
#else
	#define GetTimeBase MPI_Wtime
	#define frequency 1.0 //Wtime already returns seconds, so we divide by 1
#endif

int numRanks;
int rank;

FILE* file;

//length of number in file
int length;

//number of symbols
int N;

//total threads per rank
int threadsPerRank;

char* data; //TODO rename?

int localLength;
char* rankData;

//should contain all ones if has all permutations
//is char as will only hold 0 or 1. could probably be unsigned : byte
char* checklist;
int permutationCount;

char* fullChecklist;

//"constants" all based on file length, number of ranks, and N
//they are described and defined in allocateMemory()
int baseChars;
int overlap;
int maxLength;



pthread_t* threads;
int numPthreads;
int* threadValues;

//how many rows the local rank has
//note grid is (rowCount x gridSize)
int rowCount;

int factorial(int n) {
	
	int result = 1;
	
	for (int i = 2; i <= n; i++) {
		result *= i;
	}
	
	return result;
}

//#define UNSAFE

//converts a character to an int,
//also mapping each one lower
//e.g '1' -> 0, 'a' -> 9
int toInt(char c) {
	#ifndef UNSAFE
		//safe version
		
		int i = -1;
		
		if (c >= '1' && c <= '9') {
			i = c - '1';
		}

		if (c >= 'a' && c <= 'f') {
			i = c - 'a' + 9;
		}
		
		if (i == -1 || i >= N) {
			printf("error: invalid character %c in file\n", c);
			exit(1);
		}
		
		return i;
	
	#else
		//unsafe version that appears to be slightly faster in testing. even with -O5
		//"unsafe" really just means it assumes the input is well-formed
			
		if (c <= '9') {
			return c - '1';
		} else {
			return c - 88; //-'a'+9 = -97+9 = -88 
		}
	
	#endif
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
int** elemsList;

//used in getPermutation()
int* permuted;

//used in getNumber()
int** posList;

//used in permutationToNumber()
int** tempPermList;

//used in numberToPermutation()
//TODO maybe only initalize some of this stuff when needed
//as it will only be used in an error case
char* outString;

//returns the permutation corresponding
//to the given number k
int* getPermutation(int k) {
	int ind;
	int m = k;
	
	int* elems = elemsList[0];
	
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
int getNumber(int* perm, int id) {
	
	int* pos = posList[id];
	int* elems = elemsList[id];
	
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
//id is used to select the right memory to ensure no conflicts
//between ranks
int permutationToNumber(char* permString, int id) {
	loadIntoInt(permString, tempPermList[id]);
	int k = getNumber(tempPermList[id], id);
	
	//getNumber can return invalid permutation numbers
	//if the given string has duplicate numbers
	//in this case we return -1 to indicate it matches no permutation
	if (k >= permutationCount) {
		return -1;
	}
	
	return k;
}

int min(int a, int b) {
	return a < b ? a : b;
}

void allocateMemory() {
	
	numPthreads = threadsPerRank-1;
	
	permutationCount = factorial(N);
	
	//base characters for each rank
	baseChars = length/numRanks;
	
	//extra overlap characters per rank. as we only overlap on the
	//ends of lines, we need to overlap up to the full n-1 amount
	//i think n/2 overlap could be used if we did overlap on both sides,
	//but this is simpler and should be equivalent.
	overlap = N-1;
	
	//maximum length per rank, if it has all base chars and overlap chars
	maxLength = baseChars+overlap;
	
	//start position for this rank in the global array
	int start = rank*baseChars;
	
	//maximum end position for this rank in the global array,
	//if it has all the characters
	int maxEnd = start + maxLength;
	
	//the true end position for this rank in the global array (exclusive)
	//we cap it at the array length, so the
	//length will be cut off if it would normally extend
	//beyond the end of the array
	//we also ensure that we get the remainder of the characters
	//if we're the last rank
	int end;
	
	if (rank == numRanks-1) {
		end = length;
	} else {
		end = min(maxEnd, length);
	}
	
	//the length of this rank's share of the data
	localLength = end - start;
	
	if (threadsPerRank > localLength) {
		printf("error: threads per rank (%d) cannot be greater than local length (%d)\n", threadsPerRank, localLength);
		exit(1);
	}
	
	if (rank == 0) {
		data = malloc(length*sizeof(char));
	}
	
	threads = malloc(numPthreads*sizeof(pthread_t));
	threadValues = malloc(threadsPerRank*sizeof(int)); //we want to include mpi thread
	
	//each thread will be givin an id to tell it which thread it is
	//thread with id 0 will be the mpi thread
	for (int i = 0; i < threadsPerRank; i++) {
		threadValues[i] = i;
	}
	
	rankData = malloc(localLength*sizeof(char));
	
	//fill with zeros
	checklist = calloc(permutationCount, sizeof(char));
	
	if (rank == 0) {
		//doesn't need to be filled with zeros,
		//as it will be the result of a reduction
		fullChecklist = malloc(permutationCount*sizeof(char));
	}
	
	permuted = malloc(N*sizeof(int));
	outString = malloc(N*sizeof(char));
	
	posList = malloc(threadsPerRank*sizeof(int*));
	elemsList = malloc(threadsPerRank*sizeof(int*));
	tempPermList = malloc(threadsPerRank*sizeof(int*));
	
	for (int i = 0; i < threadsPerRank; i++) {
		posList[i] = malloc(N*sizeof(int));
		elemsList[i] = malloc(N*sizeof(int));
		tempPermList[i] = malloc(N*sizeof(int));
	}
}

void freeMemory() {
	if (rank == 0) {
		free(data);
	}
	
	free(threads);
	free(threadValues);
	
	free(rankData);
	
	free(checklist);
	
	if (rank == 0) {
		free(fullChecklist);
	}
	
	free(permuted);
	free(outString);
	
	for (int i = 0; i < threadsPerRank; i++) {
		free(posList[i]);
		free(elemsList[i]);
		free(tempPermList[i]);
	}
	
	free(posList);
	free(elemsList);
	free(tempPermList);
}

void* thing(void* arg) {
	
	
	//unpack the argument into the id
	int id = *((int*)arg);
	
	//int isMpi = id == 0;
	
	//each block is an N char substring
	int numBlocks = localLength-N+1;
	
	int blocksPerThread = numBlocks/threadsPerRank;
	
	//rank-local start and end row indecies for each thread
	int start = id*blocksPerThread;
	
	int end;
	if (id == threadsPerRank-1) {
		end = numBlocks;
	} else {
		end = min(start+blocksPerThread, numBlocks);
	}
	
	printf("rank %d thread %d: working on %d to %d\n", rank, id, start, end);
	
	
	for (int i = start; i < end; i++) {
		int k = permutationToNumber(rankData+i, id);
		
		if (k != -1) {
			checklist[k] = 1;
		}
	}
	
	return 0;
}

/*


//set up the pthread barrier for use in sycronising the pthreads to
	//have them wait for the mpi tasks to finish
	pthread_barrier_init(&barrier, NULL, totalThreads);
	
	long long startTime = GetTimeBase();
	
	//spawn threads
	for (int i = 0; i < numPthreads; i++) {
		//each pthread gets a number 1 or greater
		pthread_create(&threads[i], NULL, simulate, &threadValues[i+1]); 
	}
	
	//run the same function for the mpi rank
	//we give it id 0 to distringuish it from the pthreads
	simulate(&threadValues[0]);
	
	//at this point most threads should be terminated, but
	//we'll wait for them just to make sure
	for (int i = 0; i < numPthreads; i++) {
		pthread_join(threads[i], NULL); 
	}




*/



//the main code
void checkNumber() {
	
	long long startTime = GetTimeBase();
	
	MPI_Request receiveRequest;
	
	MPI_Irecv(rankData, localLength, MPI_CHAR, 0, 1, MPI_COMM_WORLD, &receiveRequest);
	
	if (rank == 0) {
		
		MPI_Request sendRequest;
		
		for (int i = 0; i < numRanks; i++) {
			
			//start position for the rank in the global array
			int start = i*baseChars;
		
			//maximum end position for the rank in the global array,
			//if it has all the characters
			int maxEnd = start + maxLength;
		
			//the true end position for the rank in the global array (exclusive)
			//we cap it at the array length, so the
			//length will be cut off if it would normally extend
			//beyond the end of the array
			//we also ensure that we get the remainder of the characters
			//if it's the last rank
			int end;
			
			if (i == numRanks-1) {
				end = length;
			} else {
				end = min(maxEnd, length);
			}
		
			//the length of the rank's share of the data
			//we cap the end point at the array length, so the
			//length will be cut off if it would normally extend
			//beyond the end of the array
			int sendLength = end - start;
			
			MPI_Isend(data+start, sendLength, MPI_CHAR, i, 1, MPI_COMM_WORLD, &sendRequest);
		}
		
	}
	
	MPI_Wait(&receiveRequest, MPI_STATUS_IGNORE);
	
	//spawn threads
	for (int i = 0; i < numPthreads; i++) {
		//each pthread gets a number 1 or greater
		pthread_create(&threads[i], NULL, thing, &threadValues[i+1]); 
	}
	
	//run the same function for the mpi rank
	//we give it id 0 to distringuish it from the pthreads
	thing(&threadValues[0]);
	
	
	//at this point most threads should be terminated, but
	//we'll wait for them just to make sure
	for (int i = 0; i < numPthreads; i++) {
		pthread_join(threads[i], NULL); 
	}
	
	
	
/* 	for (int i = 0; i < localLength-N+1; i++) {
		int k = permutationToNumber(rankData+i);
		
		//TODO see if we should do this second check
		//might be making code slightly slower or faster
		if (k != -1 && !checklist[k]) {
			checklist[k] = 1;
		}
		
	} */
	
	//TODO could make checklist "bytes" instead. doesn't really matter
	//would more be to note that it represents 0,1 vs an actual ascii character
	
	//do reduction across checklists using logical-or reduction
	MPI_Reduce(checklist, fullChecklist, permutationCount, MPI_CHAR, MPI_LOR, 0, MPI_COMM_WORLD);
	
	if (rank == 0) {
		
		int good = 1;
		printf("checking number...\n");
		for (int i = 0; i < permutationCount; i++) {
			if (!fullChecklist[i]) {
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
}

int main(int argc, char** argv) {
	
	MPI_Init(&argc, &argv);
	
	MPI_Comm_size(MPI_COMM_WORLD, &numRanks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	if (argc < 4) {
		if (rank == 0) {
			printf("usage: %s N inputFile threadsPerRank\n", argv[0]);
		}
		exit(1);
	}

	N = atoi(argv[1]);
	
	threadsPerRank = atoi(argv[3]);

	if (rank == 0) {
		
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
		
		printf("running with %d ranks\n", numRanks);
		
		#ifdef BGQ
			printf("compiled for BG/Q\n");
		#else
			printf("not compiled for BG/Q\n");
		#endif
		
		printf("N = %d\n", N);
		printf("file size detected as %d\n", length);
	}
	
	//send the length from rank 0 to all ranks
	MPI_Bcast(&length, 1, MPI_INT, 0, MPI_COMM_WORLD);
	
	if (numRanks > length) {
		if (rank == 0) {
			printf("error: number of ranks must not be more than the length of the file (%d > %d)\n", numRanks, length);
		}
		exit(1);
	}
	
	allocateMemory();

	if (rank == 0) {
		//read length bytes into data
		fread(data, sizeof(char), length, file);
		fclose(file);	
	}

	checkNumber();

	freeMemory();
	
	MPI_Finalize();
	return 0;
}
