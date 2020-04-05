#include <stdio.h>

int print_n(int num){
	for (int i = 0; i < num; i++){
		puts("Hello, world!");
	}
	return 0;
}

int main(int argc, char** argv) {
	print_n(5);
	return 0;
}


