#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define FILE_NAME "testFIOfile.datalifetest"
#define DATA_SIZE 64 * 1024 * 1024  // 64MB

int main() {
    FILE *file;
    unsigned char *data = malloc(DATA_SIZE);
    if (!data) {
        perror("Failed to allocate memory");
        return 1;
    }

    // Seed random number generator
    srand((unsigned)time(NULL));

    // Fill data with random bytes
    for (size_t i = 0; i < DATA_SIZE; i++) {
        data[i] = rand() % 256;
    }

    // Open file for writing
    file = fopen(FILE_NAME, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        free(data);
        return 1;
    }

    // Write data to file
    if (fwrite(data, 1, DATA_SIZE, file) != DATA_SIZE) {
        perror("Failed to write data to file");
        fclose(file);
        free(data);
        return 1;
    }
    fclose(file);
    printf("4MB of random data written to %s\n", FILE_NAME);

    // Clear data buffer
    memset(data, 0, DATA_SIZE);

    // Open file for reading
    file = fopen(FILE_NAME, "rb");
    if (!file) {
        perror("Failed to open file for reading");
        free(data);
        return 1;
    }

    // Read data from file
    if (fread(data, 1, DATA_SIZE, file) != DATA_SIZE) {
        perror("Failed to read data from file");
        fclose(file);
        free(data);
        return 1;
    }
    fclose(file);
    printf("4MB of data read back from %s\n", FILE_NAME);

    // Free allocated memory
    free(data);

    return 0;
}