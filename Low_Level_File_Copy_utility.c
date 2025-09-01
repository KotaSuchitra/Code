#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define BUF_SIZE 4096

// Read all bytes requested or return -1 on error
static ssize_t read_all(int fd, void *buffer, size_t count) {
    uint8_t *ptr = buffer;
    size_t left = count;

    while (left > 0) {
        ssize_t bytes_read = read(fd, ptr, left);
        if (bytes_read < 0) {
            if (errno == EINTR)
                continue; // Retry on interrupt
            return -1; // Other errors
        }
        if (bytes_read == 0)
            break; // EOF
        left -= bytes_read;
        ptr += bytes_read;
    }

    return (ssize_t)(count - left);
}

// Write all bytes or return -1 on error
static ssize_t write_all(int fd, const void *buffer, size_t count) {
    const uint8_t *ptr = buffer;
    size_t left = count;

    while (left > 0) {
        ssize_t bytes_written = write(fd, ptr, left);
        if (bytes_written < 0) {
            if (errno == EINTR)
                continue; // Retry on interrupt
            return -1;
        }
        left -= bytes_written;
        ptr += bytes_written;
    }

    return (ssize_t)count;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_file> <destination_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];

    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        perror("Failed to open source file");
        return EXIT_FAILURE;
    }

    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        perror("Failed to open destination file");
        close(src_fd);
        return EXIT_FAILURE;
    }

    uint8_t buffer[BUF_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(src_fd, buffer, BUF_SIZE)) > 0) {
        if (write_all(dst_fd, buffer, (size_t)bytes_read) != bytes_read) {
            perror("Write error");
            close(src_fd);
            close(dst_fd);
            return EXIT_FAILURE;
        }
    }

    if (bytes_read < 0) {
        perror("Read error");
        close(src_fd);
        close(dst_fd);
        return EXIT_FAILURE;
    }

    if (close(src_fd) < 0) {
        perror("Error closing source file");
        close(dst_fd);
        return EXIT_FAILURE;
    }

    if (close(dst_fd) < 0) {
        perror("Error closing destination file");
        return EXIT_FAILURE;
    }

    printf("File copied successfully.\n");
    return EXIT_SUCCESS;
}
