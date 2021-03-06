#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

//example compile and run:
// mpicc -Wall perm-parallel.c && mpirun -n 2 ./a.out 9 tests/valid9.in

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

//holds all the data. only used by rank 0
char* data;

//these keep track of the rank's data
int localLength;
char* rankData;

//should contain all ones if has all permutations
//is char as will only hold 0 or 1. could probably be unsigned : byte
char* checklist;
int permutationCount;

//the result of a reduce to rank 0
char* fullChecklist;

//"constants" all based on file length, number of ranks, and N
//they are described and defined in allocateMemory()
int baseChars;
int overlap;
int maxLength;

//total time for overhead
double totalOverheadTime;

//temporaries used for measuring overhead
//and computation time
//these don't really need to be global, but
//it's a little cleaner that way
long long tempStart;
long long tempEnd;

//array storing all the reduce times
double* reduceTimes;

//array storing all the computation times
double* computationTimes;

//converts the start and end number of ticks into
//time in seconds
double makeTime(long long start, long long end) {
	return ((double)(end-start))/frequency;
}

//starts a timer by recording the time
void startTimer() {
	tempStart = GetTimeBase();
}

//stops the timer by recording the time,
//then returns the difference in seconds
double stopGetTimer() {
	tempEnd = GetTimeBase();
	return makeTime(tempStart, tempEnd);
}

//returns n!
//used for setting permutationCount
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
int* elems;

//used in getPermutation()
int* permuted;

//used in getNumber()
int* pos;

//used in permutationToNumber()
int* tempPerm;

//used in numberToPermutation()
char* outString;

//returns the permutation corresponding
//to the given number k
//uses Antoine Comeau's version of
//Keith Schwarz's Factoradic Permutation Algorithm
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

//given a number, returns a permutation as
//a non-null terminated string of length N
char* numberToPermutation(int k) {
	int* p = getPermutation(k);
	
	loadIntoString(p, outString);
	
	return outString;
}

//returns the number corresponding to the given permutation
//uses Antoine Comeau's version of
//Keith Schwarz's Factoradic Permutation Algorithm
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

//returns the min of ints a and b
int min(int a, int b) {
	return a < b ? a : b;
}

//allocates all dynamically needed memory
void allocateMemory() {
	
	permutationCount = factorial(N);
	
	if (rank == 0) {
		data = malloc(length*sizeof(char));
	}
	
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
	
	rankData = malloc(localLength*sizeof(char));
	
	//fill with zeros
	checklist = calloc(permutationCount, sizeof(char));
	
	if (rank == 0) {
		//doesn't need to be filled with zeros,
		//as it will be the result of a reduction
		fullChecklist = malloc(permutationCount*sizeof(char));
	}
	
	elems = malloc(N*sizeof(int));
	permuted = malloc(N*sizeof(int));
	pos = malloc(N*sizeof(int));
	
	tempPerm = malloc(N*sizeof(int));
	outString = malloc(N*sizeof(char));
	
	if (rank == 0) {	
		reduceTimes = malloc(numRanks*sizeof(double));
		computationTimes = malloc(numRanks*sizeof(double));
	}
}

//frees all dynamically allocated memory
void freeMemory() {
	
	if (rank == 0) {
		free(data);
	}
	
	free(rankData);
	
	free(checklist);
	
	if (rank == 0) {
		free(fullChecklist);
	}
	
	free(elems);
	free(permuted);
	free(pos);
	
	free(tempPerm);
	free(outString);
	
	if (rank == 0) {
		free(reduceTimes);
		free(computationTimes);
	}
}

//the main body of the code. does the checking if
//the given permutation is valid or not
void checkNumber() {
	
	//start timing for the total time
	//must be declared outside the if (rank == 0) {}
	//so the compiler doesn't get angry
	//even though it's only used by rank 0
	long long startTime = GetTimeBase();
	
	if (rank == 0) {
		//start timing for overhead for all these isends and irecvs
		startTimer();
	}
	
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
	
	//add to overhead time
	if (rank == 0) {
		double isendIrecvTime = stopGetTimer();
		printf("isend/irecv time:\n%f\n\n", isendIrecvTime);
		totalOverheadTime += isendIrecvTime;
	}

	//start timing the main calculation loop
	startTimer();
	
	//same as in serial, just using locak rank data
	for (int i = 0; i < localLength-N+1; i++) {
		int k = permutationToNumber(rankData+i);
		
		if (k != -1) {
			checklist[k] = 1;
		}
		
	}
	
	double rankComputationTime = stopGetTimer();
	
	//start timing for the reduce
	//although unintuitive, the reduce actually does
	//take different amounts of time for each rank, so we
	//will need to find the longest time in a bit
	startTimer();
	
	//do reduction across checklists using logical-or reduction
	MPI_Reduce(checklist, fullChecklist, permutationCount, MPI_CHAR, MPI_LOR, 0, MPI_COMM_WORLD);
	
	double rankReduceTime = stopGetTimer();

	//collect all the recorded times so we can find the largest times
	MPI_Gather(&rankComputationTime, 1, MPI_DOUBLE, computationTimes, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	MPI_Gather(&rankReduceTime, 1, MPI_DOUBLE, reduceTimes, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	
	if (rank == 0) {
		
		//find the rank with the largest time, and it's time
		//both for computation and reduce times
		
		double maxReduceTime = reduceTimes[0];
		int maxReduceTimeIndex = 0;
		
		double maxComputationTime = computationTimes[0];
		int maxComputationTimeIndex = 0;
		
		for (int i = 1; i < numRanks; i++) {
			
			if (reduceTimes[i] > maxReduceTime) {
				maxReduceTime = reduceTimes[i];
				maxReduceTimeIndex = i;
			}

			if (computationTimes[i] > maxComputationTime) {
				maxComputationTime = computationTimes[i];
				maxComputationTimeIndex = i;
			}
		}

		printf("max reduce time (from rank %d):\n%f\n\n", maxReduceTimeIndex, maxReduceTime);
		
		totalOverheadTime += maxReduceTime;
		
		//start timing for rank 0 checking the checklist
		startTimer();
		
		//same as serial, but using fullChecklist
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
			printf("number is good!\n\n");
		} else {
			printf("number is not good\n\n");
		}
		
		double checkingTime = stopGetTimer();
		double totalComputationTime = maxComputationTime + checkingTime;
		
		long long endTime = GetTimeBase();
		double totalTime = makeTime(startTime, endTime);
		
		printf("total overhead time: \n%f\n\n", totalOverheadTime);
		printf("max computation time (from rank %d):\n%f\n\n", maxComputationTimeIndex, totalComputationTime);
		
		printf("total time:\n%f\n\n", totalTime);
	}
}

int main(int argc, char** argv) {
	MPI_Init(&argc, &argv);
	
	MPI_Comm_size(MPI_COMM_WORLD, &numRanks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	if (argc < 3) {
		if (rank == 0) {
			printf("usage: %s N inputFile\n", argv[0]);
		}
		exit(1);
	}

	N = atoi(argv[1]);

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
		printf("file size detected as %d\n\n", length);
		
		totalOverheadTime = 0;

		//start timing to capture the broadcast below
		startTimer();
	}
	
	//send the length from rank 0 to all ranks
	MPI_Bcast(&length, 1, MPI_INT, 0, MPI_COMM_WORLD);
	
	if (numRanks > length) {
		if (rank == 0) {
			printf("error: number of ranks must not be more than the length of the file (%d > %d)\n", numRanks, length);
		}
		exit(1);
	}
	
	//add time to overhead
	if (rank == 0) {
		double broadcastTime = stopGetTimer();
		printf("broadcast time: \n%f\n\n", broadcastTime);
		totalOverheadTime += broadcastTime;
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
