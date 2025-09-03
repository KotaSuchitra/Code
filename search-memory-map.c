#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

void search_keyword(const char *filename, const char *keyword) {
    int fd;
    struct stat sb;
    char *file_in_memory;
    size_t keyword_len = strlen(keyword);

    // Open the file
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Get the file size
    if (fstat(fd, &sb) == -1) {
        perror("Error getting file size");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Memory-map the file
    file_in_memory = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_in_memory == MAP_FAILED) {
        perror("Error mapping file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Search for keyword
    char *current = file_in_memory;
    char *end = file_in_memory + sb.st_size;
    int found = 0;
    while (current <= end - keyword_len) {
        if (memcmp(current, keyword, keyword_len) == 0) {
            size_t position = current - file_in_memory;
            printf("Keyword found at offset %zu\n", position);
            found = 1;
        }
        current++;
    }

    if (!found) {
        printf("Keyword not found.\n");
    }

    // Cleanup
    if (munmap(file_in_memory, sb.st_size) == -1) {
        perror("Error unmapping file");
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <keyword>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    search_keyword(argv[1], argv[2]);

    return 0;
}
