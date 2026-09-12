#include "backend/regAllocator/regAllocator.h"
#include "helper/filereader.h"
#include <stdio.h>
#include "tester/testGen.h"
#include <windows.h>
#include <string.h>

int main(int argc, char* argv[]){
	if(!strcmp(argv[1], "RESET_TESTS")){
		clearTestSuites(); return 0;
	}
	jmp_buf compRetEnv;
	if(!setjmp(compRetEnv)){
		setJmpBuf(compRetEnv);
		verifyAlphabeticalOrder();
		char* src = loadFileToBuffer(argv[1]);
		tokenArray a = tokenizeSource(src);
		printf("%d\n", CopyFileA(argv[1], pathSrc, FALSE));
		printf("\x1b[1;32mTokenized Source\x1b[0m\n\n\n");
		node b = constructTree(a);
		printf("\x1b[1;32mConstructed AST\x1b[0m\n");
		printf("\n\n");
		printf("\x1b[36mAST\x1b[0m\n\n");
		switchFile(PAST);
		printTree(&b, 0);
		printf("\n\n");
		printf("\x1b[1;32mGenerated 3AC\x1b[0m\n\n");
		printf("\x1b[36m3AC IR\x1b[0m\n\n");
		switchFile(P3AC);
		arena quads = linearizeAST(&b);
		sizedPool ranges = constructRanges(quads);
		printf("\n\n");
		printf("\x1b[1;32mFound Live Ranges\x1b[0m\n\n");
		printf("\x1b[36mRanges\x1b[0m\n\n");
		switchFile(PRANGE);
		printRanges(ranges);
		printf("\n\n");
		arena edges = constructEdges(ranges);
		printf("\x1b[1;32mConstructed Edges\x1b[0m\n\n");
		printf("\x1b[36mEdges\x1b[0m\n\n");
		switchFile(PEDGE);
		printEdges(edges);
		if(argc > 2 && !strcmp(argv[2], "TEST")) queryStore();
	} else{
		fprintf(stderr, "ERRORS ENCOUNTERED IN COMPILATION\n\n");
	}
	return 0;
}