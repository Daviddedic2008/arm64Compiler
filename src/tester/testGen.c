#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include "testGen.h"

char* srcPrgm; uint32_t charsUsed = 2048;

const char* sourcePath = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\test.c";

BOOL SavePersistentInt(const char* valueName, DWORD value) {
    HKEY hKey;
    long status = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\davidscompiler1234", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (status != ERROR_SUCCESS) return FALSE;
    status = RegSetValueExA(hKey, valueName, 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
    RegCloseKey(hKey);
    return (status == ERROR_SUCCESS) ? TRUE : FALSE;
}
BOOL LoadPersistentInt(const char* valueName, DWORD* outValue) {
    HKEY hKey;
    long status = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\davidscompiler1234", 0, KEY_READ, &hKey);
    if (status != ERROR_SUCCESS) {
        *outValue = 0; 
        return FALSE;
    }

    DWORD size = sizeof(DWORD);
    status = RegGetValueA(hKey, NULL, valueName, RRF_RT_REG_DWORD, NULL, outValue, &size);
    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS) {
        *outValue = 0;
        return FALSE;
    }

    return TRUE;
}

void switchFile(testFile newFile){
	if(curFile != NULL) fclose(curFile);
	curFile = fopen(paths[newFile], "w");
}

void printfD(const char* specifier, ...){
	va_list args;
    va_start(args, specifier);
	vfprintf(curFile, specifier, args);
	vprintf(specifier, args);
	va_end(args);
}

void storeRecentRun(){
    char baseDir[MAX_PATH]; 
    DWORD nt = 0;
    
    LoadPersistentInt("numTestsSaved", &nt);
    nt++; 
    SavePersistentInt("numTestsSaved", nt);

	if (curFile != NULL) {
        fflush(curFile);
        fclose(curFile);
        curFile = NULL;
    }
    // 1. Build destination directory path
    snprintf(baseDir, sizeof(baseDir), "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\testSuites\\%lutest", nt);

    if (!CreateDirectoryA(baseDir, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            printf("[ERROR] Failed to create directory: %lu\n", err);
            return;
        }
    }

    const char* files[] = {"src.txt", "3AC.txt", "AST.txt", "EDGES.txt", "RANGES.txt"};
    char srcPath[MAX_PATH];
    char destPath[MAX_PATH];
	
    for (int i = 0; i < 5; i++) {
        snprintf(srcPath, sizeof(srcPath), "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\%s", files[i]);
        snprintf(destPath, sizeof(destPath), "%s\\%s", baseDir, files[i]);

        if (!CopyFileA(srcPath, destPath, FALSE)) {
            printf("[ERROR] Copying failed for %s! Error Code: %lu\n", files[i], GetLastError());
        } else {
            printf("Successfully archived: %s\n", files[i]);
        }
    }
}

void queryStore(){
	uint8_t store = 0;
	char buffer[10];
	printf("\033[0;33mStore as test? (YES / NO) > \033[0m");
	scanf(" %3s", buffer);
	if(buffer[0] == 'Y' || buffer[0] == 'y'){
		storeRecentRun();
	}
}

void DeleteDirectoryContents(const char* dirPath){
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", dirPath);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if(hFind == INVALID_HANDLE_VALUE) return;

    do{
        if(strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0){
            continue;
        }

        char filePath[MAX_PATH];
        snprintf(filePath, sizeof(filePath), "%s\\%s", dirPath, findData.cFileName);

        if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            DeleteDirectoryContents(filePath);
            RemoveDirectoryA(filePath);
        } else{
            DeleteFileA(filePath);
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}
void clearTestSuites(){
    const char* targetDir = "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\testSuites";

    printf("\033[0;31mClearing all test suites and resetting counter...\033[0m\n");
    DeleteDirectoryContents(targetDir);
    if(SavePersistentInt("numTestsSaved", 0)){
        printf("Registry counter successfully reset to 0.\n");
    }else{
        printf("[ERROR] Failed to reset Registry counter.\n");
    }

    printf("Cleanup complete!\n");
}

#include <stdio.h>
#include <string.h>

#define CHUNK_SIZE 8192

int diff_files(const char *path1, const char *path2) {
    FILE *f1 = fopen(path1, "rb");
    FILE *f2 = fopen(path2, "rb");

    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return 1;
    }
    fseek(f1, 0, SEEK_END);
    fseek(f2, 0, SEEK_END);
    if (ftell(f1) != ftell(f2)) {
        fclose(f1);
        fclose(f2);
        return 1;
    }
    rewind(f1);
    rewind(f2);

    unsigned char buf1[CHUNK_SIZE];
    unsigned char buf2[CHUNK_SIZE];
    size_t bytes1, bytes2;

    do {
        bytes1 = fread(buf1, 1, CHUNK_SIZE, f1);
        bytes2 = fread(bytes1 > 0 ? buf2 : buf2, 1, bytes1, f2); // Read identical amounts

        if (bytes1 != bytes2 || memcmp(buf1, buf2, bytes1) != 0) {
            fclose(f1);
            fclose(f2);
            return 1; // Difference found
        }
    } while (bytes1 > 0);

    fclose(f1);
    fclose(f2);
    return 0; // Identical
}


uint8_t compareOutputs(const uint32_t testNumber){
    const char* files[] = {"src.txt", "3AC.txt", "AST.txt", "EDGES.txt", "RANGES.txt"};
    char srcPath[MAX_PATH];
    char destPath[MAX_PATH];
    char baseDir[MAX_PATH];
    
    snprintf(baseDir, sizeof(baseDir), "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\testSuites\\%lutest", (unsigned long)testNumber);
    
    for (int i = 0; i < 5; i++) {
        snprintf(srcPath, sizeof(srcPath), "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\lastRun\\%s", files[i]);
        snprintf(destPath, sizeof(destPath), "%s\\%s", baseDir, files[i]);
        
        if (diff_files(srcPath, destPath)) {
            printf("\033[0;31m[MISMATCH] Difference in file <<%s>>\033[0m\n", files[i]); 
            return 1;
        }
    } 
    return 0;
}

void runTests(){
    DWORD numTests = 0;
    if (!LoadPersistentInt("numTestsSaved", &numTests) || numTests == 0) {
        printf("\033[0;33mNo archived test suites found to run.\033[0m\n");
        return;
    }

    printf("\033[1;36m=== Running %lu Archived Test Suite(s) ===\033[0m\n\n", numTests);
    
    uint32_t passed = 0;
    uint32_t failed = 0;
    char command[MAX_PATH * 2];
    char testSrcPath[MAX_PATH];

    for (DWORD i = 1; i <= numTests; i++) {
        snprintf(testSrcPath, sizeof(testSrcPath), "C:\\CMake_Projects\\CortexM4CompilerV2\\src\\tester\\testSuites\\%lutest\\src.txt", (unsigned long)i);
        
        snprintf(command, sizeof(command), "..\\..\\compile.bat \"%s\"", testSrcPath);

        printf("[TEST %2lu/%2lu] Executing...", i, numTests);
        fflush(stdout);

        system(command);

        if (compareOutputs(i) == 0) {
            printf(" \033[1;32m[PASSED]\033[0m\n");
            passed++;
        } else {
            printf(" \033[0;31m[FAILED]\033[0m\n");
            failed++;
        }
    }

    printf("\n\033[1;36m=== Test Run Summary ===\033[0m\n");
    printf("Total Suites: %lu | \033[1;32mPassed: %u\033[0m | \033[0;31mFailed: %u\033[0m\n\n", numTests, passed, failed);
}

int main(){
	runTests();
}