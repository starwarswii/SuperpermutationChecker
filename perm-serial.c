#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

//loads a string into an int array
//e.g '1' becomes 0
void loadIntoInt(char* string, int* array) {
	for (int i = 0; i < N; i++) {
		array[i] = string[i] - '0' - 1;
	}
}

//loads an int array into a string
//e.g. 0 becomes '1'
void loadIntoString(int* array, char* string) {
	for (int i = 0; i < N; i++) {
		string[i] = array[i] + '0' + 1;
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
	printf("the string is %.*s\n", N, permString);
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
	
	printf("checklist is %d, which is %d!\n", permutationCount, N);
	
	for (int i = 0; i < length-N+1; i++) {
		int k = permutationToNumber(data+i);
		
		
		for (int j = 0; j < N; j++) {
			printf("looking at data %d\n", i+j);
		}
		printf("\n");
		
		printf("k is %d, less than size %d\n", k, permutationCount);
		
		if (k >= permutationCount) {
			printf("==============================woah!, that's not supposed to happen!\n");
			exit(69);
		}
		
		if (k != -1 && !checklist[k]) {
			checklist[k] = 1;
		}
		
	}
	
	int good = 1;
	printf("checking number...\n");
	for (int i = 0; i < permutationCount; i++) {
		if (!checklist[i]) {
			char* perm = numberToPermutation(i);
			printf("missing permutation %d: %.*s\n", i, N, perm);
			good = 0;
		}
	}
	
	if (good) {
		printf("number is good!\n");
	} else {
		printf("number is not good\n");
	}
	
}

int main(int argc, char** argv) {

	if (argc < 3) {
		printf("usage: %s N inputFile\n", argv[0]);
		exit(1);
	}


	N = atoi(argv[1]);

	file = fopen(argv[2], "r");
	if (file == NULL) {
		printf("could not open input file %s/n", argv[2]);
		exit(1);
	}

	//we get the length of the file by seeking to the end and
	//then checking where we are
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	
	//then we go back to the start of the file
	fseek(file, 0, SEEK_SET);
	
	printf("file size detected as %d\n", length);

	allocateMemory();

	//read length bytes into data
	fread(data, sizeof(char), length, file);
	fclose(file);

	
	checkNumber();
	
	
	
	
	
	//char* string = malloc(N*sizeof(char));
	
	// int max = factorial(N);
	
	// for (int i = 0; i < max+1; i++) {
		// char* p = numberToPermutation(i);
		
		// int k = permutationToNumber(p);
		
		//prints non-null terminated string of length N
		// printf("permutation %d=%d: %.*s\n", i, k, N, p);
	// }
	
	//free(string);
	

	freeMemory();
	
	return 0;
}
