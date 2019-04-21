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
		//int posPerm = pos[perm[i]];
		
		k += m * pos[perm[i]];
		m *= (N-i);
		
		pos[elems[N-i-1]] = pos[perm[i]];
		elems[pos[perm[i]]] = elems[N-i-1];
	}
	
	return k;
}

//returns the number corresponding to the given permutation string
int permutationToNumber(char* permString) {
	loadIntoInt(permString, tempPerm);
	return getNumber(tempPerm);
}


void allocateMemory() {
	data = malloc(length*sizeof(char));
	
	elems = malloc(N*sizeof(int));
	permuted = malloc(N*sizeof(int));
	pos = malloc(N*sizeof(int));
	
	tempPerm = malloc(N*sizeof(int));
	outString = malloc(N*sizeof(char));
	
	//used in permutationToNumber()
}

void freeMemory() {
	free(data);
	
	free(elems);
	free(permuted);
	free(pos);
	
	free(tempPerm);
	free(outString);
}

int main(int argc, char** argv) {

	if (argc < 4) {
		printf("usage: %s N inputFile length\n", argv[0]);
		exit(1);
	}


	N = atoi(argv[1]);

	file = fopen(argv[2], "r");
	if (file == NULL) {
		printf("could not open input file %s/n", argv[1]);
		exit(1);
	}

	length = atoi(argv[3]);
	
	allocateMemory();

	//read length bytes into data
	fread(data, sizeof(char), length, file);
	fclose(file);

	
	
	
	//TODO actually check data

	
	
	
	//char* string = malloc(N*sizeof(char));
	
	int max = 4*3*2*1;
	
	for (int i = 0; i < max+1; i++) {
		char* p = numberToPermutation(i);
		
		int k = permutationToNumber(p);
		
		//prints non-null terminated string of length N
		printf("permutation %d=%d: %.*s\n", i, k, N, p);
	}
	
	//free(string);
	

	freeMemory();
	
	return 0;
}
