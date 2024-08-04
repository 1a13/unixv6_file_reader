#include "directory.h"
#include "direntv6.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>

static const int DIRENTS_PER_BLOCK = DISKIMG_SECTOR_SIZE / sizeof(struct direntv6);

int directory_findname(const struct unixfilesystem *fs, const char *name,
                       int dirinumber, struct direntv6 *dirEnt) {
    // get inode of directory to get size
    struct inode inp;
    if (inode_iget(fs, dirinumber, &inp) != 0) {
        fprintf(stderr, "Error: Inode read improperly.\n");
        return -1;
    }

    // loop through each block index of the directory
    for (int i = 0; i * DISKIMG_SECTOR_SIZE < inode_getsize(&inp); i++) {
        // read in blocks from inode
        struct direntv6 buf[DIRENTS_PER_BLOCK];
        int blockSize = file_getblock(fs, dirinumber, i, buf);
        if (blockSize == -1) {
            fprintf(stderr, "Error getting block at index %d\n", i);
            return -1;
        }

        // loop through each dirent in block to find matching name
        int numDir = blockSize / sizeof(struct direntv6);   // avoid accessing invalid dirents
        for (int j = 0; j < numDir; j++) {
            if (strncmp(name, buf[j].d_name, MAX_COMPONENT_LENGTH) == 0) {
                *dirEnt = buf[j];
                return 0;
            }
        }
    }

    // name not found in directory 
    return -1;
}
