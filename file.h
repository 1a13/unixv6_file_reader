/* This file defines the file-layer function for fetching the data
 * for a specified part of a file.
 */

#ifndef _FILE_H_
#define _FILE_H_

#include "unixfilesystem.h"

/**
 * Fetches the specified file block from the specified inode and
 * stores it at buf. FileBlockIndex is the index of a data block within
 * a file: 0 means the first data block, 1 means the second data block,
 * and so on (these data blocks could be referenced directly within the
 * inode, or in a singly- or doubly- indirect block).
 * Returns the number of valid bytes in the returned block;
 * this will be the same as the sector size, except for the last
 * block of a file.  Returns -1 if an error occurs in this function
 * or any function it calls.
 */
int file_getblock(const struct unixfilesystem *fs, int inumber,
                  int fileBlockIndex, void *buf);

#endif // _FILE_H_
