#include <stdio.h>
#include <stdarg.h>

static const char* const path3AC = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\3AC.txt";
static const char* const pathAST = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\AST.txt";
static const char* const pathEdges = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\EDGES.txt";
static const char* const pathRanges = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\RANGES.txt";
static const char* const pathSrc = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\src.txt";

typedef enum{P3AC, PAST, PEDGE, PRANGE, PSRC}testFile;

static const char* paths[] = {[PSRC] = pathSrc, [P3AC] = path3AC, [PAST] = pathAST, [PEDGE] = pathEdges, [PRANGE] = pathRanges}; 

static FILE* curFile;

void printfD(const char* specifier, ...);

void switchFile(testFile newFile);

void queryStore();

void clearTestSuites();