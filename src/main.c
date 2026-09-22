#include "backend/regAllocator/regAllocator.h"
#include "helper/filereader.h"
#include <stdio.h>
#include "tester/testGen.h"
#include <windows.h>
#include <string.h>

#define printfCND(a) if(getFlush()) printf(a)

int main(int argc, char* argv[]){
	if(!strcmp(argv[1], "RESET_TESTS")){
		clearTestSuites(); return 0;
	}
	jmp_buf compRetEnv;
	if(!setjmp(compRetEnv)){
		if(argc > 3){
			assignToFlush(!strcmp(argv[3], "DEBUG"));
		}
		else if(argc > 2){
			assignToFlush(flushStdout = !strcmp(argv[2], "DEBUG"));
		}
		setJmpBuf(compRetEnv);
		verifyAlphabeticalOrder();
		char* src = loadFileToBuffer(argv[1]);
		tokenArray a = tokenizeSource(src);
		CopyFileA(argv[1], pathSrc, FALSE);
		printfCND("\x1b[1;32mTokenized Source\x1b[0m\n\n\n");
		node b = constructTree(a);
		printfCND("\x1b[1;32mConstructed AST\x1b[0m\n");
		printfCND("\n\n");
		printfCND("\x1b[36mAST\x1b[0m\n\n");
		switchFile(PAST);
		printTree(&b, 0);
		printfCND("\n\n");
		printfCND("\x1b[1;32mGenerated 3AC\x1b[0m\n\n");
		printfCND("\x1b[36m3AC IR\x1b[0m\n\n");
		switchFile(P3AC);
		arena quads = linearizeAST(&b);
		constructRanges(quads);
		printfCND("\n\n");
		printfCND("\x1b[1;32mFound Live Ranges\x1b[0m\n\n");
		printfCND("\x1b[36mRanges\x1b[0m\n\n");
		switchFile(PRANGE);
		printRanges();
		printfCND("\n\n");
		constructEdges();
		printfCND("\x1b[1;32mConstructed Edges\x1b[0m\n\n");
		printfCND("\x1b[36mEdges\x1b[0m\n\n");
		switchFile(PEDGE);
		printEdges();
		if(argc > 2 && !strcmp(argv[2], "TEST")) queryStore();
	} else{
		fprintf(stderr, "ERRORS ENCOUNTERED IN COMPILATION\n\n");
	}
	return 0;
}