#include <stdio.h>

#include "inode.h"
#include "diskimg.h"
#include <stdbool.h>
#include <limits.h>

#define INODE_BLOCK 2
#define NUM_SGL_INDIR_BLOCKS 7

static const int INODES_PER_BLOCK = DISKIMG_SECTOR_SIZE / sizeof(struct inode);
static const int SINGLY_INDIRECT_BYTES = 7 * (DISKIMG_SECTOR_SIZE / sizeof(uint16_t))
                                         * DISKIMG_SECTOR_SIZE;
static const int BLOCKNUMS_PER_BLOCK = DISKIMG_SECTOR_SIZE / sizeof(uint16_t);

/* This function reads the data for the inode with the specified
   inumber from the filesystem and fills in an inode struct with 
   that information. This function will return 0 opon success,
   or -1 if there is an error.
 */
int inode_iget(const struct unixfilesystem *fs, int inumber,
        struct inode *inp) {
    int sectorNum = INODE_BLOCK + (inumber - 1) / INODES_PER_BLOCK;
    struct inode buf[INODES_PER_BLOCK];
    int bytes = diskimg_readsector(fs->dfd, sectorNum, buf);
    
    if (bytes == -1) {
        fprintf(stderr, "Error reading in sector. No bytes read\n");
        return -1;
    }

    int indexInBlock = (inumber - 1) % INODES_PER_BLOCK;
    *inp = buf[indexInBlock];
    
    return 0;
}

/* This function returns the block number of the file data block at the 
   specified index.
 */
int inode_indexlookup(const struct unixfilesystem *fs, struct inode *inp,
        int fileBlockIndex) {
    int fileSize = inode_getsize(inp);
    if (fileBlockIndex * DISKIMG_SECTOR_SIZE > fileSize || fileBlockIndex < 0) {
        fprintf(stderr, "Invalid file block index.\n");
        return -1;
    }
    
    // small mode
    if ((inp->i_mode & ILARG) == 0) {
        return inp->i_addr[fileBlockIndex];
    }

    // large mode
    int blockNum = fileBlockIndex / BLOCKNUMS_PER_BLOCK;
    int iaddrIndex = blockNum;
    if (blockNum > NUM_SGL_INDIR_BLOCKS) {
        iaddrIndex = NUM_SGL_INDIR_BLOCKS;
    }

    uint16_t indirectBlock = inp->i_addr[iaddrIndex];
    uint16_t buf[BLOCKNUMS_PER_BLOCK];
    int bytes = diskimg_readsector(fs->dfd, indirectBlock, buf);

    if (bytes == -1) {
        fprintf(stderr, "Error reading in sector. No bytes read\n");
        return -1;
    }

    // singly indirect block
    if (inode_getsize(inp) <= SINGLY_INDIRECT_BYTES || blockNum < NUM_SGL_INDIR_BLOCKS) {
        return buf[fileBlockIndex % BLOCKNUMS_PER_BLOCK];
    
    // doubly indirect block
    } else {
        int secondIndex = blockNum - NUM_SGL_INDIR_BLOCKS;  // reset indexes at 0
        bytes = diskimg_readsector(fs->dfd, buf[secondIndex], buf);

        if (bytes == -1) {
            fprintf(stderr, "Error reading in sector. No bytes read\n");
            return -1;
        }
        
        return buf[fileBlockIndex % BLOCKNUMS_PER_BLOCK];
    }
}

int inode_getsize(struct inode *inp) {
    return ((inp->i_size0 << (sizeof(inp->i_size1) * CHAR_BIT)) | inp->i_size1);
}
