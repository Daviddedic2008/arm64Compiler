#include <stdio.h>

const char* path3AC = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\3AC.txt";
const char* pathAST = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\AST.txt";
const char* pathEdges = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\EDGES.txt";
const char* pathRanges = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\RANGES.txt";

typedef enum{P3AC, PAST, PEDGE, PRANGE};

const char* paths[] = {[P3AC] = path3AC, [PAST] = pathAST, [PEDGE] = pathEdges, [PRANGE] = pathRanges}; 

FILE* curFile;