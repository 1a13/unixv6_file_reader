#include <stdio.h>

#include "file.h"
#include "inode.h"
#include "diskimg.h"

/* This function reads a block of data from a file, given the file's
   i-number and the desired block within the file.
 */
int file_getblock(const struct unixfilesystem *fs, int inumber,
        int fileBlockIndex, void *buf) {
    struct inode inp;
    if (inode_iget(fs, inumber, &inp) != 0) {
        fprintf(stderr, "Error: Inode read improperly.\n");
        return -1;
    }

    int blockNum = inode_indexlookup(fs, &inp, fileBlockIndex);
    if (blockNum == -1) {
        fprintf(stderr, "Error: Invalid index.\n");
        return -1;
    }

    int fileSize = inode_getsize(&inp);
    int bytes = diskimg_readsector(fs->dfd, blockNum, buf);
    if (bytes == -1) {
        fprintf(stderr, "Error: Data read in improperly.\n");
        return -1;
    }

    // check if block is in last index of file
    if ((fileSize / DISKIMG_SECTOR_SIZE) == fileBlockIndex) {
        return fileSize % DISKIMG_SECTOR_SIZE;
    }
    return bytes;
}