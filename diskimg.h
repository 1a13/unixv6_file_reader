#ifndef _DISKIMG_H_
#define _DISKIMG_H_

#include <stdint.h>

// Size of a disk sector (e.g. block) in bytes.
#define DISKIMG_SECTOR_SIZE 512

/**
 * Opens a disk image for I/O. Returns an open file descriptor, or -1 if
 * unsuccessful.  
 */
int diskimg_open(const char *pathname, int readOnly);

/**
 * Returns the size in bytes of the disk image specified by the given disk
 * file descriptor, or -1 if unsuccessful.
 */
int diskimg_getsize(int dfd);

/**
 * Reads the specified sector (e.g. sectorNum) from the disk image specified
 * by the given disk file descriptor and stores it at buf.
 * Returns the number of bytes read, or -1 on error.
 */
int diskimg_readsector(int dfd, int sectorNum, void *buf);

/**
 * Writes the information at buf to the specified sector on the disk image
 * specified by the given disk file descriptor.
 * Returns the number of bytes written, or -1 on error.
 */
int diskimg_writesector(int dfd, int sectorNum, const void *buf);

/**
 * Clean up from a previous diskimg_open() call.  Returns 0 on success,
 * or -1 on error.
 */
int diskimg_close(int dfd);

#endif // _DISKIMG_H_
