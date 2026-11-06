#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int file_backed_shm_open(const char *name, int flags, mode_t mode) {
    const char *directory = getenv("WITCHER2_REX_BACKING_DIR");
    if (!directory || strncmp(name, "/xenia_memory_", 14) != 0) {
        return shm_open(name, flags, mode);
    }
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s%s.XXXXXX", directory, name);
    if (length < 0 || length >= sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    int fd = mkstemp(path);
    if (fd >= 0 && unlink(path) != 0) {
        int error = errno;
        close(fd);
        errno = error;
        return -1;
    }
    return fd;
}

__attribute__((used, section("__DATA,__interpose")))
static const struct {
    const void *replacement;
    const void *original;
} shim = {file_backed_shm_open, shm_open};
