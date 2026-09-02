#include "backend/regAllocator/regAllocator.h"
#include "helper/filereader.h"
#include <stdio.h>

int main(int argc, char* argv[]){
	jmp_buf compRetEnv;
	if(!setjmp(compRetEnv)){
		setJmpBuf(compRetEnv);
		verifyAlphabeticalOrder();
		char* src = loadFileToBuffer(argv[1]);
		tokenArray a = tokenizeSource(src);
		node b = constructTree(a);
		printf("\x1b[1;32mConstructed AST\x1b[0m\n");
		printf("\n\n");
		printf("\x1b[36mAST\x1b[0m\n\n");
		printTree(&b, 0);
		printf("\n\n");
		printf("\x1b[1;32mGenerated 3AC\x1b[0m\n\n");
		printf("\x1b[36m3AC IR\x1b[0m\n\n");
		arena quads = linearizeAST(&b);
		sizedPool ranges = constructRanges(quads);
		printf("\n\n");
		printf("\x1b[1;32mFound Live Ranges\x1b[0m\n\n");
		printf("\x1b[36mRanges\x1b[0m\n\n");
		printRanges(ranges);
	} else{
		fprintf(stderr, "ERRORS ENCOUNTERED IN COMPILATION\n\n");
	}
	return 0;
}