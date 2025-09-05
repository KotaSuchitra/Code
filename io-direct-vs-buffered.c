#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <fcntl.h>

#ifndef __linux__
#define O_DIRECT 0
#endif

static void fatal(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

long long to_ns(struct timespec *t)
{
    return (long long)t->tv_sec * 1000000000LL + t->tv_nsec;
}

long long diff_ns(struct timespec *a, struct timespec *b)
{
    return to_ns(b) - to_ns(a);
}

void fill_buf(unsigned char *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        buf[i] = (unsigned char)(i & 0xFF);
    }
}

void bench_write(const char *label, int fd, unsigned char *buf, size_t wsize, size_t iters, int do_fsync)
{
    struct timespec t0, t1;
    long long min = (1LL << 62), max = 0, sum = 0;

    printf("WRITE: %s wsize=%zu iters=%zu fsync=%d\n", label, wsize, iters, do_fsync);

    for (size_t i = 0; i < iters; i++)
    {
        if (clock_gettime(CLOCK_MONOTONIC, &t0) < 0)
        {
            fatal("clock_gettime");
        }

        ssize_t w = write(fd, buf, wsize);
        if (w < 0)
        {
            fatal("write");
        }
        if ((size_t)w != wsize)
        {
            fprintf(stderr, "short write %zd/%zu\n", w, wsize);
            return;
        }

        if (do_fsync && fsync(fd) < 0)
        {
            fatal("fsync");
        }

        if (clock_gettime(CLOCK_MONOTONIC, &t1) < 0)
        {
            fatal("clock_gettime");
        }

        long long ns = diff_ns(&t0, &t1);
        if (ns < min)
            min = ns;
        if (ns > max)
            max = ns;
        sum += ns;
    }

    double avg = (double)sum / iters;
    double mb = (double)wsize * iters / (1024.0 * 1024.0);
    double sec = (double)sum / 1e9;

    printf("RESULT %s ops=%zu MB=%.2f avg=%.2fms min=%.2fms max=%.2fms thr=%.2fMB/s\n",
           label, iters, mb, avg / 1e6, min / 1e6, max / 1e6, sec > 0 ? (mb / sec) : 0);
}

void bench_read(const char *label, int fd, unsigned char *buf, size_t rsize, size_t iters)
{
    struct timespec t0, t1;
    long long min = (1LL << 62), max = 0, sum = 0;

    printf("READ: %s rsize=%zu iters=%zu\n", label, rsize, iters);

    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        fatal("lseek");
    }

    for (size_t i = 0; i < iters; i++)
    {
        if (clock_gettime(CLOCK_MONOTONIC, &t0) < 0)
        {
            fatal("clock_gettime");
        }

        ssize_t r = read(fd, buf, rsize);
        if (r < 0)
        {
            fatal("read");
        }
        if ((size_t)r != rsize)
        {
            fprintf(stderr, "short read %zd/%zu\n", r, rsize);
            return;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &t1) < 0)
        {
            fatal("clock_gettime");
        }

        long long ns = diff_ns(&t0, &t1);
        if (ns < min)
            min = ns;
        if (ns > max)
            max = ns;
        sum += ns;
    }

    double avg = (double)sum / iters;
    double mb = (double)rsize * iters / (1024.0 * 1024.0);
    double sec = (double)sum / 1e9;

    printf("RESULT %s ops=%zu MB=%.2f avg=%.2fms min=%.2fms max=%.2fms thr=%.2fMB/s\n",
           label, iters, mb, avg / 1e6, min / 1e6, max / 1e6, sec > 0 ? (mb / sec) : 0);
}

int main(int argc, char **argv)
{
    const char *file = "/tmp/test_direct_io.bin";
    size_t wsize = 4096, total_mb = 64;

    if (argc > 1)
        file = argv[1];
    if (argc > 2)
        wsize = (size_t)atoi(argv[2]);

    size_t iters = (total_mb * 1024 * 1024) / wsize;
    if (!iters)
    {
        fprintf(stderr, "bad params\n");
        return 1;
    }

    printf("file=%s wsize=%zu MB=%zu iters=%zu\n", file, wsize, total_mb, iters);

    // buffered write
    int fd_buf = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd_buf < 0)
    {
        fatal("open buf");
    }

    unsigned char *buf = malloc(wsize);
    if (!buf)
    {
        fatal("malloc");
    }
    fill_buf(buf, wsize);

    bench_write("buffered", fd_buf, buf, wsize, iters, 1);
    close(fd_buf);

    // direct write
    long page = sysconf(_SC_PAGESIZE);
    size_t align = page;

#ifdef __linux__
    int fd_dir = open(file, O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT, 0644);
#elif __APPLE__
    int fd_dir = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd_dir >= 0)
        fcntl(fd_dir, F_NOCACHE, 1);
#endif
    if (fd_dir < 0)
    {
        fatal("open direct");
    }

    void *dbuf = NULL;
    if (posix_memalign(&dbuf, align, wsize))
    {
        fatal("posix_memalign");
    }
    fill_buf(dbuf, wsize);

    bench_write("direct", fd_dir, dbuf, wsize, iters, 1);
    close(fd_dir);

    // buffered read
    int fd_rbuf = open(file, O_RDONLY);
    if (fd_rbuf < 0)
    {
        fatal("open rbuf");
    }
    unsigned char *rbuf = malloc(wsize);
    if (!rbuf)
    {
        fatal("malloc");
    }
#ifdef __linux__
    posix_fadvise(fd_rbuf, 0, 0, POSIX_FADV_DONTNEED);
#endif
    bench_read("buf(cold)", fd_rbuf, rbuf, wsize, iters);
    close(fd_rbuf);

    // direct read
    int fd_rdir = open(file, O_RDONLY | O_DIRECT);
    if (fd_rdir < 0)
    {
        fatal("open rdir");
    }
    void *rdbuf = NULL;
    if (posix_memalign(&rdbuf, align, wsize))
    {
        fatal("posix_memalign rdbuf");
    }
    bench_read("direct", fd_rdir, rdbuf, wsize, iters);
    close(fd_rdir);

    free(buf);
    free(dbuf);
    free(rbuf);
    free(rdbuf);

    printf("done\n");
    return 0;
}
