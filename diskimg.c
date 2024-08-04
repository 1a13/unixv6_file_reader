#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "diskimg.h"

int diskimg_open(const char *pathname, int readOnly) {
    return open(pathname, readOnly ? O_RDONLY : O_RDWR);
}

int diskimg_getsize(int dfd) {
    return lseek(dfd, 0, SEEK_END);
}

int diskimg_readsector(int dfd, int sectorNum, void *buf) {
    if (lseek(dfd, sectorNum * DISKIMG_SECTOR_SIZE, SEEK_SET) == (off_t) -1) {
        return -1;
    }
    return read(dfd, buf, DISKIMG_SECTOR_SIZE);
}

int diskimg_writesector(int dfd, int sectorNum, const void *buf) {
    if (lseek(dfd, sectorNum * DISKIMG_SECTOR_SIZE, SEEK_SET) == (off_t) -1) {
      return -1;
    }
    return write(dfd, buf, DISKIMG_SECTOR_SIZE);
}

int diskimg_close(int dfd) {
    return close(dfd);
}
