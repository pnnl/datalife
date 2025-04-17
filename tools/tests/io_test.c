#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FILE_NAME "testIOfile.datalifetest"
#define DATA_SIZE 64 * 1024 * 1024  // 64MB

int main() {
    int file;
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
    file = open(FILE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == -1) {
        perror("Failed to open file for writing");
        free(data);
        return 1;
    }

    // Write data to file
    ssize_t bytes_written = write(file, data, DATA_SIZE);
    if (bytes_written != DATA_SIZE) {
        perror("Failed to write data to file");
        close(file);
        free(data);
        return 1;
    }
    close(file);
    printf("4MB of random data written to %s\n", FILE_NAME);

    // Clear data buffer
    memset(data, 0, DATA_SIZE);

    // Open file for reading
    file = open(FILE_NAME, O_RDONLY);
    if (file == -1) {
        perror("Failed to open file for reading");
        free(data);
        return 1;
    }

    // Read data from file
    ssize_t bytes_read = read(file, data, DATA_SIZE);
    if (bytes_read != DATA_SIZE) {
        perror("Failed to read data from file");
        close(file);
        free(data);
        return 1;
    }
    close(file);
    printf("4MB of data read back from %s\n", FILE_NAME);

    // Free allocated memory
    free(data);

    return 0;
}