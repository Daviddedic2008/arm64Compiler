#include <stdio.h>
#include <stdlib.h>

char* srcPrgm; uint32_t charsUsed = 2048;

const char* srcPath = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\test.c";

void readSrc(){
	FILE *file = fopen(srcPath, "rb");
    if (!file) return 1;
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);
    srcPrgm = malloc(fileSize + 1);
    if (!srcPrgm) {
        fclose(file);
        return 1;
    }
    size_t bytesRead = fread(srcPrgm, 1, fileSize, file);
    srcPrgm[bytesRead] = '\0';
    fclose(file);
}

void switchFile(testFile newFile){
	if(curFile != NULL) fclose(curFile);
	curFile = fopen(paths[newFile], "w");
}
