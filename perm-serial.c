#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

FILE* file;


//length of number in file
int length;

//number of symbols
int N;

char* data; //TODO rename

//loads a string into an int array
void loadIntoInt(char* string, int* array) {
	for (int i = 0; i < N; i++) {
		array[i] = string[i] - '0';
	}
}

//loads an int array into a string
void loadIntoString(int* array, char* string) {
	for (int i = 0; i < N; i++) {
		string[i] = array[i] + '0';
	}
}

//returns the permutation corresponding
//to the given number k
int* perm(int k) {
	int ind;
	int m = k;
	
	int* permuted = malloc(N*sizeof(int));
	int* elems = malloc(N*sizeof(int));
	
	for (int i = 0; i < N; i++) {
		elems[i] = i; //added+1
	}
	
	for (int i = 0; i < N; i++) {
		ind = m % (N-i);
		m /= (N-i); // added /=
		
		permuted[i] = elems[ind];
		elems[ind] = elems[N-i-1];
	}
	
	free(elems);
	
	return permuted;
}

//returns the number corresponding to the given permutation
int inv(int* perm) {
	
	int k = 0;
	int m = 1;
	
	int* pos = malloc(N*sizeof(int));
	int* elems = malloc(N*sizeof(int));
	
	for (int i = 0; i < N; i++) {
		pos[i] = i;
		elems[i] = i; //added +1
	}
	
	for (int i = 0; i < N-1; i++) {
		//int posPerm = pos[perm[i]];
		
		k += m * pos[perm[i]];
		m *= (N-i); //added *=
		
		pos[elems[N-i-1]] = pos[perm[i]];
		elems[pos[perm[i]]] = elems[N-i-1];
	}
	
	free(pos);
	free(elems);
	
	return k;
}


void allocateMemory() {
	data = malloc(length*sizeof(char));
}

void freeMemory() {
	free(data);
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

	
	freeMemory();
	
	//TODO actually check data

	
	
	
	char* string = malloc(N*sizeof(char));
	
	int max = 4*3*2*1;
	
	for (int i = 0; i < max+1; i++) {
		int* p = perm(i);
		loadIntoString(p, string);
		
		int k = inv(p);
		
		free(p);
		
		//prints non-null terminated string of length N
		printf("permutation %d=%d: %.*s\n", i, k, N, string);
	}
	
	free(string);
	

	return 0;
}
