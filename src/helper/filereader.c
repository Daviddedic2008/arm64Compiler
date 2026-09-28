#include <stdio.h>
#include <stdlib.h>

char* loadFileToBuffer(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    size_t bytesRead = fread(buffer, 1, size, file);
    fclose(file);

    buffer[bytesRead] = '\0';

    return buffer;
}