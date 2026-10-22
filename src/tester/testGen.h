#include <stdio.h>
#include <stdarg.h>

static const char* const path3AC = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\3AC.txt";
static const char* const pathAST = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\AST.txt";
static const char* const pathSrc = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\SRC.c";
static const char* const pathReg = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\REGALLOC.txt";
static const char* const pathPar = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\PAR.txt";

typedef enum{P3AC, PAST, PEDGE, PRANGE, PSRC, PREG, PAR, ENDFILE}testFile;

static const char* paths[] = {[PSRC] = pathSrc, [P3AC] = path3AC, [PAST] = pathAST, [PREG] = pathReg, [PAR] = pathPar}; 

static FILE* curFile; static bool flushStdout;
void assignToFlush(const bool b);
bool getFlush();

void printfD(const char* specifier, ...);

void switchFile(testFile newFile);

void queryStore();

void clearTestSuites();