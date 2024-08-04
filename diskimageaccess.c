/* FILE: diskimageaccess.c
 * --------------------
 * Based on diskimageaccess.c testing program written
 * and contributed to by many CS110/CS111 instructors.
 * Also based on function_tester.c testing program
 * written by Nick Troccoli and test.c testing program
 * written by John Ousterhout.
 *
 * This file is a test harness program for the V6FS 
 * assignment.  It allows individual testing of each
 * assignment function as well as more comprehensive
 * testing of the entire filesystem.  Run with -h
 * to see its usage and available tests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "diskimg.h"
#include "unixfilesystem.h"
#include "inode.h"
#include "file.h"
#include "directory.h"
#include "pathname.h"
#include "chksumfile.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))


static const int INODES_PER_BLOCK = DISKIMG_SECTOR_SIZE / sizeof(struct inode);


/* Function: print_block_checksum
 * ------------------------------
 * This function reads in the specified file block index of the specified inode
 * using file_getblock and prints out the checksum of its data, which relies
 * on the data and the return value of file_getblock.
 */
static void print_block_checksum(const struct unixfilesystem *fs, int inumber, int blockIndex) {
  // Print the checksum for this block's contents
  char buf[DISKIMG_SECTOR_SIZE];
  int bytes = file_getblock(fs, inumber, blockIndex, buf);
  if (bytes < 0) {
    printf("file_getblock(%d, %d) returned %d\n", inumber, blockIndex, bytes);
  } else {
    char chksum_str[CHKSUMFILE_STRINGSIZE];
    int result = chksumblock(fs, buf, bytes, chksum_str);
    if (result == -1) {
      printf("Inode %d can't compute checksum for block %d due to checksum library error\n", inumber, blockIndex);
    } else {
      printf("  %d -> %s\n", blockIndex, chksum_str);
    }
  }
}

/* Function: inode_or_file_layer_test
 * ----------------------------------
 * This function is taken from diskimageaccess.c; it can be used to test 
 * inode_iget, inode_indexlookup, and file_getblock.
 *
 * It iterates through all inodes on the disk and reads them in with
 * inode_iget, reporting any errors.  For allocated ones, it prints the inode
 * information.
 *
 * If include_mappings is true, for all allocated inodes this also prints up to
 * 10 of its initial and ending fileBlockIndex mappings (calls 
 * inode_indexlookup for fileBlockIndexes 0-10 and n-10 to n).
 *
 * If include_file_layer_checksums is true, for all allocated inodes this also
 * prints the full file data checksum (calls file_getblock on all blocks in
 * the file and checksums them all together).  If include_mappings is also
 * true, instead of printing the mapping as mentioned above, it prints a
 * per-block checksum.
 */
static void inode_or_file_layer_test(const struct unixfilesystem *fs, bool include_mappings, bool include_file_layer_checksums) {
  // Check each inode
  for (int inumber = ROOT_INUMBER; inumber <= fs->superblock.s_isize * INODES_PER_BLOCK; inumber++) {
    struct inode in;
    if (inode_iget(fs, inumber, &in) < 0) {
      printf("inode_iget(%d) returned < 0\n", inumber);
      return;
    }

    // Skip this inode if it's not allocated.
    if ((in.i_mode & IALLOC) == 0) {
      continue;
    }
    
    int size = inode_getsize(&in);
    printf("Inode %d mode 0x%x size %d", inumber, in.i_mode, size);

    // Full file layer checksum
    if (include_file_layer_checksums) {
      char chksum[CHKSUMFILE_SIZE];
      int error_bno;
      int chksumresult = chksumfile_byinumber_error_checking(fs, inumber, chksum, &error_bno);
      if (chksumresult < 0) {
        if (chksumresult == -1) {
          printf("\n\t->ERROR: Inode %d can't compute full file checksum; inode_iget(%d) returned < 0\n", inumber, inumber);
        } else if (chksumresult == -2) {
          printf("\n\t->ERROR: Inode %d can't compute full file checksum; file_getblock(%d, %d) returned < 0\n", inumber, inumber, error_bno);
        } else {
          printf("\n\t->ERROR: Inode %d can't compute full file checksum; checksum library error\n", inumber);
        }
        continue;
      }
      
      char chksumstring[CHKSUMFILE_STRINGSIZE];
      chksumfile_cvt2string(chksum, chksumstring);
      printf(" full file checksum = %s\n", chksumstring);
    }

    printf("\n");

    // Print out per-block information for this file
    if (include_mappings && size > 0) {
      int numMappings = (size + DISKIMG_SECTOR_SIZE - 1)/DISKIMG_SECTOR_SIZE;
      printf("Inode %d: calling inode_indexlookup on first %d fileBlockIndex(es):\n", inumber, min(10, numMappings));
      for (int blockIndex = 0; blockIndex < min(10, numMappings); blockIndex++) {
        // If no checksums, just print the block number mapping
        if (!include_file_layer_checksums) {
          printf("  inode_indexlookup(fileBlockIndex=%d) = %d\n", blockIndex, inode_indexlookup(fs, &in, blockIndex));
        } else {
          print_block_checksum(fs, inumber, blockIndex);
        }
      }

      // Print trailing mappings (if any)
      if (numMappings > 10) {
        printf("Inode %d: calling inode_indexlookup on last %d fileBlockIndex(es):\n", inumber, max(10, numMappings - 10));
      } else {
        printf("Inode %d: That's everything! It's a relatively small file!\n", inumber);
      }
      for (int blockIndex = max(10, numMappings - 10); blockIndex < numMappings; blockIndex++) {
        // If no checksums, just print the block number mapping
        if (!include_file_layer_checksums) {
          printf("  inode_indexlookup(fileBlockIndex=%d) = %d\n", blockIndex, inode_indexlookup(fs, &in, blockIndex));
        } else {
          print_block_checksum(fs, inumber, blockIndex);
        }
      }
    }
  }
}


/***** TESTING INODE_IGET *****/


/* Function: test_inode_iget_custom
 * ----------------------------------
 * This function tests the inode_iget function
 * with custom parameter values; it error checks for if
 * the specified inumber is invalid for this filesystem, and otherwise
 * prints the return value of inode_iget along with the fields in the inode
 * that was read in and what they say about the inode (e.g. directory,
 * what mapping scheme, free or allocated).
 */
static void test_inode_iget_custom(const struct unixfilesystem *fs, int inumber) {
  printf("Calling inode_iget(%d)\n-----\n", inumber);

  // Bounds check
  int max_inode_number = fs->superblock.s_isize * INODES_PER_BLOCK;
  if (inumber < ROOT_INUMBER || inumber > max_inode_number) {
    printf("ERROR: invalid inumber for this disk; ");
    printf("must be between %d and %d, inclusive\n", ROOT_INUMBER, max_inode_number);
    return;
  }

  // Read in the inode
  struct inode in;
  int result = inode_iget(fs, inumber, &in);
  if (result == -1) {
    printf("inode_iget(%d) returned -1\n", inumber);
    return;
  }

  // Print out the inode fields
  printf("struct inode {\n");
  printf("\ti_mode = %d,\n", in.i_mode);
  printf("\ti_nlink = %d,\n", in.i_nlink);
  printf("\ti_uid = %d,\n", in.i_uid);
  printf("\ti_gid = %d,\n", in.i_gid);
  printf("\tsize = %d,\n", inode_getsize(&in));
  for (size_t i = 0; i < sizeof(in.i_addr) / sizeof(in.i_addr[0]); i++) {
    printf("\ti_addr[%lu] = %d,\n", i, in.i_addr[i]);
  }
  printf("\ti_atime = %d %d,\n", in.i_atime[0], in.i_atime[1]);
  printf("\ti_mtime = %d %d\n", in.i_mtime[0], in.i_mtime[1]);
  printf("}\n\n");

  // Print inode information from its i_mode
  if ((in.i_mode & IALLOC) != 0) {
    printf("This inode is in use.\n");

    if ((in.i_mode & IFMT) == IFDIR) {
      printf("This inode represents a directory.\n");
    } else {
      printf("This inode does not represent a directory.\n");
    }

    if ((in.i_mode & ILARG) != 0) {
      printf("This inode uses the large mapping scheme.\n");
    } else {
      printf("This inode uses the small mapping scheme.\n");
    }
  } else {
    printf("This inode is free.\n");
  }
}

/* Function: test_inode_iget
 * ----------------------------------
 * This function handles all testing for inode_iget; it expects one string
 * argument, which can be either "test1" or an inode number.
 * If "test1", this function runs a test that prints out info about all
 * allocated inodes on the disk via calls to inode_iget.  
 * Otherwise, it calls inode_iget with the specified
 * inode number parsed to an int and prints out info about the 
 * inode struct given back.
 */
static void test_inode_iget(const struct unixfilesystem *fs, const char *arg) {
  // custom test
  if (strcmp(arg, "test1") != 0) {
    test_inode_iget_custom(fs, atoi(arg));
    return;
  }

  // test1
  printf("test1: printing info for all allocated inodes on this disk (%d inodes total)\n", 
    fs->superblock.s_isize * INODES_PER_BLOCK);
  printf("(if an inode is not printed, this means its i_mode field states it is not used)\n\n");
  inode_or_file_layer_test(fs, false, false);
}


/***** TESTING INODE_INDEXLOOKUP *****/


/* Function: test_inode_indexlookup_custom
 * ----------------------------------
 * This function tests the inode_indexlookup function with custom arguments; 
 * it error checks for if the specified inumber is invalid for this filesystem,
 * if this inode is free (and therefore we cannot read its block numbers), or 
 * if inode_iget returned -1, and otherwise prints the return value of 
 * inode_indexlookup.
 */
static void test_inode_indexlookup_custom(const struct unixfilesystem *fs, 
  int inumber, int fileBlockIndex) {

  printf("Calling inode_indexlookup(%d, %d)\n-----\n", 
    inumber, fileBlockIndex);

  // Bounds check
  int max_inode_number = fs->superblock.s_isize * INODES_PER_BLOCK;
  if (inumber < ROOT_INUMBER || inumber > max_inode_number) {
    printf("ERROR: invalid inumber for this disk; ");
    printf("must be between %d and %d, inclusive\n", ROOT_INUMBER, max_inode_number);
    return;
  }

  // Read in the inode
  struct inode in;
  int result = inode_iget(fs, inumber, &in);
  if (result == -1) {
    printf("inode_iget(%d) returned -1\n", inumber);
    return;
  }

  if ((in.i_mode & IALLOC) == 0) {
    printf("ERROR: this inode is marked free, can't access block numbers.\n");
    return;
  }
  
  result = inode_indexlookup(fs, &in, fileBlockIndex);
  printf("inode_indexlookup(inumber = %d, fileBlockIndex = %d) returned %d\n", 
    inumber, fileBlockIndex, result);
}

/* Function: test_inode_indexlookup
 * ----------------------------------
 * This function handles all testing for inode_indexlookup; it expects an args
 * array, which can either contain one element "testX" (1 <= X <= 4), or two
 * elements: the first an inode number, the second a fileBlockIndex.
 *
 * NOTE: test1/test2/test3 ignore the 'fs' parameter and always run with
 * basicDiskImageExtended, as the tests are specific to its contents.
 *
 * If "test1": runs a test to look up the first block of a small file
 * If "test2": runs a test to look up the first block of a large file
 * If "test3": runs a test to look up the last block of a doubly-indirect 
               large file
 * If "test4": runs a test for inode_indexlookup on all files on the 
 *             specified disk
 *
 * Otherwise, it calls inode_indexlookup with the specified
 * inode number and fileBlockIndex parsed to ints and prints out the return
 * value.
 */
static void test_inode_indexlookup(const struct unixfilesystem *fs, const char *args[]) {
  // test1/test2/test3 (these ignore fs and always use basicDiskImageExtended)
  if (!strcmp(args[0], "test1") || !strcmp(args[0], "test2") || !strcmp(args[0], "test3")) {
    printf("%s: opening basicDiskImageExtended (overrides command-line disk name)\n", args[0]);
    
    // Open basicDiskImageExtended
    const char *diskpath = "samples/disk_images/basicDiskImageExtended";
    int fd = diskimg_open(diskpath, 1);
    if (fd < 0) {
      printf("Can't open diskimagePath %s\n", diskpath);
      return;
    }
    struct unixfilesystem *fs2 = unixfilesystem_init(fd);
    if (!fs2) {
      printf("Failed to initialize unix filesystem\n");
    }
    
    if (!strcmp(args[0], "test1")) {
      printf("test1: get the block number for inode 1 (small file), fileBlockIndex 0 (tests direct addressing)\n\n");
      test_inode_indexlookup_custom(fs2, 1, 0);
    } else if (!strcmp(args[0], "test2")) {
      printf("test2: get the block number for inode 5 (large file), fileBlockIndex 1200 (tests singly-indirect addressing)\n\n");
      test_inode_indexlookup_custom(fs2, 5, 1200);
    } else if (!strcmp(args[0], "test3")) {
      printf("test3: get the block number for inode 5 (large file), fileBlockIndex 10000 (tests doubly-indirect addressing)\n\n");
      test_inode_indexlookup_custom(fs2, 5, 10000);
    }

    // Close the disk image when we're done
    int err = diskimg_close(fd);
    if (err < 0) {
      printf("Error closing %s\n", diskpath);
    }
    free(fs2);
  } else if (!strcmp(args[0], "test4")) {
    // Uses disk image specified by user
    printf("test4: printing block number info for all allocated inodes on this disk (%d inodes total)\n", 
      fs->superblock.s_isize * INODES_PER_BLOCK);
    printf("(if an inode is not printed, this means its i_mode field states it is not used)\n\n");
    inode_or_file_layer_test(fs, true, false);
  } else {
    // Custom test
    test_inode_indexlookup_custom(fs, atoi(args[0]), atoi(args[1]));
  }
}


/***** TESTING FILE_GETBLOCK *****/


/* Function: test_file_getblock_custom
 * ----------------------------------
 * This function tests the file_getblock function with custom parameters; it 
 * error checks for if the specified inumber is invalid for this filesystem, 
 * if this inode is free (and therefore we cannot read its blocks), or if 
 * inode_iget returned -1, and otherwise prints the return value of 
 * inode_indexlookup and the contents of the block in hexadecimal.
 */
static void test_file_getblock_custom(const struct unixfilesystem *fs, int inumber, 
  int fileBlockIndex) {

  printf("Calling file_getblock(%d, %d)\n-----\n", 
    inumber, fileBlockIndex);

  // Bounds check
  int max_inode_number = fs->superblock.s_isize * 16;
  if (inumber < 1 || inumber > max_inode_number) {
    printf("ERROR: invalid inumber for this disk; ");
    printf("must be between 1 and %d, inclusive\n", max_inode_number);
    return;
  }

  // Read in the inode for error checking purposes
  struct inode in;
  int result = inode_iget(fs, inumber, &in);
  if (result == -1) {
    printf("inode_iget(%d) returned -1\n", inumber);
    return;
  }

  if ((in.i_mode & IALLOC) == 0) {
    printf("ERROR: this inode is marked as free, cannot access its blocks.\n");
    return;
  }

  // Read the block
  char buf[DISKIMG_SECTOR_SIZE];
  result = file_getblock(fs, inumber, fileBlockIndex, buf);
  printf("file_getblock(inumber = %d, fileBlockIndex = %d) returned %d\n", 
    inumber, fileBlockIndex, result);
  if (result == -1) {
    return;
  }

  /* Print the file block in hex, 16 digits per line, padded to 2 digits
   * (padding from StackOverflow post 13275258, hh from 3555791)
   */
  printf("File block contents:\n\n");
  for (int i = 0; i < result; i++) {
    if (i % 16 == 0 && i != 0) printf("\n");
    printf("%02hhx ", buf[i]);
  }
  printf("\n");
}

/* Function: test_file_getblock
 * ----------------------------------
 * This function handles all testing for file_getblock; it expects an args
 * array, which can either contain one element "testX" (1 <= X <= 4), or two
 * elements: the first an inode number, the second a fileBlockIndex.
 *
 * NOTE: test1/test2/test3 ignore the 'fs' parameter and always run with
 * basicDiskImageExtended, as the tests are specific to its contents.
 *
 * If "test1": runs a test to read in the first block of a small file
 * If "test2": runs a test to read in the first block of a large file
 * If "test3": runs a test to read in the last block of a doubly-indirect 
               large file
 * If "test4": runs a test for file_getblock on all files on the 
 *             specified disk
 *
 * Otherwise, it calls file_getblock with the specified
 * inode number and fileBlockIndex parsed to ints and prints out the return
 * value.
 */
static void test_file_getblock(const struct unixfilesystem *fs, const char *args[]) {
  // test1/test2/test3 (these ignore fs and always use basicDiskImageExtended)
  if (!strcmp(args[0], "test1") || !strcmp(args[0], "test2") || !strcmp(args[0], "test3")) {
    printf("%s: opening basicDiskImageExtended (overrides command-line disk name)\n", args[0]);
    
    // Open basicDiskImageExtended
    const char *diskpath = "samples/disk_images/basicDiskImageExtended";
    int fd = diskimg_open(diskpath, 1);
    if (fd < 0) {
      printf("Can't open diskimagePath %s\n", diskpath);
      return;
    }
    struct unixfilesystem *fs2 = unixfilesystem_init(fd);
    if (!fs2) {
      printf("Failed to initialize unix filesystem\n");
    }
    
    if (!strcmp(args[0], "test1")) {
      printf("test1: get the block data for inode 1 (small file), fileBlockIndex 0 and print it out in hex (direct addressing)\n\n");
      test_file_getblock_custom(fs2, 1, 0);
    } else if (!strcmp(args[0], "test2")) {
      printf("test2: get the block data for inode 5 (large file), fileBlockIndex 1200 and print it out in hex (singly-indirect addressing)\n\n");
      test_file_getblock_custom(fs2, 5, 1200);
    } else if (!strcmp(args[0], "test3")) {
      printf("test3: get the block data for inode 5 (large file), fileBlockIndex 10000 and print it out in hex (doubly-indirect addressing)\n\n");
      test_file_getblock_custom(fs2, 5, 10000);
    }

    // Close the disk image when we're done
    int err = diskimg_close(fd);
    if (err < 0) {
      printf("Error closing %s\n", diskpath);
    }
    free(fs2);
  } else if (!strcmp(args[0], "test4")) {
    // Uses disk image specified by user
    printf("test4: printing info for all allocated inodes on this disk (%d inodes total)\n", 
      fs->superblock.s_isize * INODES_PER_BLOCK);
    printf("If an inode is not printed, this means its i_mode field states it is not used.\n");
    printf("Instead of printing all the block data for every block, this test prints a *checksum* value\n");
    printf("for each block's data, as well as a checksum value for the entire file's data.  A checksum is a single\n");
    printf("number representation of data that only matches if the data matches.  If the entire file checksum\n");
    printf("doesn't match, inspect the per-block checksums for mismatches.  If a per-block checksum doesn't\n");
    printf("match, investigate that particular call further to ensure the data is read in correctly,\n");
    printf("the return value is correct (# bytes read - this impacts the checksum!), and inode_indexlookup returns the right value.\n\n");
    inode_or_file_layer_test(fs, true, true);
  } else {
    // Custom test
    test_file_getblock_custom(fs, atoi(args[0]), atoi(args[1]));
  }
}


/***** TESTING DIRECTORY_FINDNAME *****/


/* Function: test_file_getblock_custom
 * ----------------------------------
 * This function tests the directory_findname function with custom parameters;
 * it error checks for if the specified inumber is invalid for this filesystem,
 * if this inode is free (and therefore we cannot read its blocks), if this 
 * inode is not a directory, or if inode_iget returned -1, and otherwise prints 
 * the contents of the direntv6 that was read in.
 */
static void test_directory_findname_custom(const struct unixfilesystem *fs, 
  int dirinumber, const char *name) {

  printf("Calling directory_findname(%d, \"%s\")\n-----\n", 
    dirinumber, name);

  // Bounds check
  int max_inode_number = fs->superblock.s_isize * 16;
  if (dirinumber < 1 || dirinumber > max_inode_number) {
    printf("ERROR: invalid inumber for this disk; ");
    printf("must be between 1 and %d, inclusive\n", max_inode_number);
    return;
  }

  // Read in the inode for error checking purposes
  struct inode in;
  int result = inode_iget(fs, dirinumber, &in);
  if (result == -1) {
    printf("inode_iget(%d) returned -1\n", dirinumber);
    return;
  }

  // Ensure that this inode is allocated and a directory
  if ((in.i_mode & IALLOC) == 0) {
    printf("ERROR: this inode is marked as free, cannot access its blocks.\n");
    return;
  } else if ((in.i_mode & IFMT) != IFDIR) {
    printf("ERROR: this inode does not represent a directory.\n");
    return;
  }

  // Read the dirent
  struct direntv6 entry;
  result = directory_findname(fs, name, dirinumber, &entry);
  if (result == -1) {
    printf("directory_findname(dirinumber = %d, name = '%s') returned -1\n", 
      dirinumber, name);
  } else {
    printf("direntv6 {\n");
    printf("\td_inumber = %d,\n", entry.d_inumber);
    printf("\td_name = '%.14s'\n", entry.d_name);
    printf("}\n");
  }
}

/* Function: print_dirent
 * -----------------------
 * (Written by John Ousterhout) This function calls directory_findname
 * with the specified parameters and prints out the dirent, or an error
 * message if directory_findname failed.
 */
void print_dirent(const struct unixfilesystem *fs, const char *name, int inumber) {
  struct direntv6 dir_ent;
  int result;

  result = directory_findname(fs, name, inumber, &dir_ent);
  if (result != 0) {
    printf("directory_findname failed for \"%s\" in inode %d\n",
      name, inumber);
    return;
  }
  printf("Directory entry for \"%s\" in inode %d contains d_name "
    "\"%.*s\", d_inumber %d\n",
    name, inumber, MAX_COMPONENT_LENGTH, dir_ent.d_name,
    dir_ent.d_inumber);
}

/* Function: test_directory_findname_test3
 * ---------------------------------------
 * This function prints all the dirents on the BasicDiskImageExtended
 * disk image (assumed to be fs; otherwise, this function will not work as
 * expected).
 */
static void test_directory_findname_test3(const struct unixfilesystem *fs) {
  print_dirent(fs, "bigfile", 1);
  print_dirent(fs, "medfile", 1);
  print_dirent(fs, "o", 1);
  print_dirent(fs, "verybig", 1);
  print_dirent(fs, "very long name", 1);
  print_dirent(fs, "foo", 1);
  print_dirent(fs, "Root", 6);
  print_dirent(fs, "Repository", 6);
  print_dirent(fs, "Entries", 6);
  print_dirent(fs, "XXX", 10);
  print_dirent(fs, "CVS", 10);
  print_dirent(fs, "Root", 12);
  print_dirent(fs, "Repository", 12);
  print_dirent(fs, "Entries", 12);
}

/* Function: test_directory_findname
 * ----------------------------------
 * This function handles all testing for directory_findname; it expects an args
 * array, which can either contain one element "testX" (1 <= X <= 3), or two
 * elements: the first a dirinumber, the second a dirent name.
 *
 * NOTE: test1/test2/test3 ignore the 'fs' parameter and always run with
 * basicDiskImageExtended, as the tests are specific to its contents.
 *
 * If "test1": runs a test to read in the first block of a small file
 * If "test2": runs a test to read in the first block of a large file
 * If "test3": runs a test to read in the last block of a doubly-indirect 
               large file
 * If "test4": runs a test for file_getblock on all files on the 
 *             specified disk
 *
 * Otherwise, it calls file_getblock with the specified
 * inode number and fileBlockIndex parsed to ints and prints out the return
 * value.
 */
static void test_directory_findname(const struct unixfilesystem *fs, const char *args[]) {
  // test1/test2/test3 (these ignore fs and always use basicDiskImageExtended)
  if (!strcmp(args[0], "test1") || !strcmp(args[0], "test2") || !strcmp(args[0], "test3")) {
    printf("%s: opening basicDiskImageExtended (overrides command-line disk name)\n", args[0]);
    
    // Open basicDiskImageExtended
    const char *diskpath = "samples/disk_images/basicDiskImageExtended";
    int fd = diskimg_open(diskpath, 1);
    if (fd < 0) {
      printf("Can't open diskimagePath %s\n", diskpath);
      return;
    }
    struct unixfilesystem *fs2 = unixfilesystem_init(fd);
    if (!fs2) {
      printf("Failed to initialize unix filesystem\n");
    }
    
    if (!strcmp(args[0], "test1")) {
      printf("test1: look for deleted.txt, an entry that was deleted, so it should not be found.\n");
      printf("This test checks if directory_findname looks at invalid dirents in a partially-filled block;\n");
      printf("deleted.txt does appear, but only after the valid dirents in that block, so it should not be found.\n");
      printf("If it is found, that means that directory_findname is scanning too many directory entries.\n\n");
      
      struct direntv6 dirEnt;
      const char *name = "deleted.txt";
      if (directory_findname(fs2, name, 1, &dirEnt) == 0) {
        printf("ERROR: file \"%s\" was found at inode %d\n", name, dirEnt.d_inumber);
      } else {
        printf("PASSED: file \"%s\" was not found.\n", name);
      }
    } else if (!strcmp(args[0], "test2")) {
      printf("test2: look for a valid 14-character filename.  If it's not found, that means that the\n");
      printf("string comparison isn't quite right, possibly regarding the length.\n\n");
      
      const char *name = "very long name";
      struct direntv6 dirEnt;
      if (directory_findname(fs2, name, 1, &dirEnt) == 0 && dirEnt.d_inumber == 6) {
        printf("PASSED: file \"%s\" was found at inode %d\n", name, dirEnt.d_inumber);
      } else if (dirEnt.d_inumber != 6) {
        printf("ERROR: file \"%s\" was found, but at incorrect inode %d (should be 6)\n", name, dirEnt.d_inumber);
      } else {
        printf("ERROR: file \"%s\" was not found.\n", name);
      }
    } else if (!strcmp(args[0], "test3")) {
      printf("test3: printing directory_findname information for all files on this disk.\n\n");
      test_directory_findname_test3(fs2);
    }

    // Close the disk image when we're done
    int err = diskimg_close(fd);
    if (err < 0) {
      printf("Error closing basicDiskImageExtended\n");
    }
    free(fs2);
  } else {
    // Custom test
    test_directory_findname_custom(fs, atoi(args[0]), args[1]);
  }
}


/***** TESTING PATHNAME_LOOKUP *****/


/* Function: test_pathname_lookup_custom
 * -------------------------------------
 * This function tests the pathname_lookup function with custom parameters;
 * it error checks for if the specified path is empty or not an absolute path
 * beginning with "/", and otherwise prints the return value of
 * pathname_lookup.  Returns the return value, or -1 on error.
 */
static int test_pathname_lookup_custom(const struct unixfilesystem *fs, 
  const char *pathname) {

  printf("Calling pathname_lookup(\"%s\")\n-----\n", pathname);

  // Must be absolute path
  if (strlen(pathname) == 0 || pathname[0] != '/') {
    printf("ERROR: pathname_lookup requires absolute paths ");
    printf("(must begin with /)\n");
    return -1;
  }

  int result = pathname_lookup(fs, pathname);
  printf("pathname_lookup(\"%s\") returned %d\n", pathname, result);
  return result;
}

/* Function: getDirEntries
 * ----------------------------------
 * This function is taken from diskimageaccess.c.  It fetches as many
 * entries from the specified directory as can fit in the specified array,
 * and returns the number of entries found.
 */
static int getDirEntries(const struct unixfilesystem *fs, int inumber, struct direntv6 *entries, int maxNumEntries) {
  struct inode in;
  int err = inode_iget(fs, inumber, &in);
  if (err < 0) return err;
  
  if (!(in.i_mode & IALLOC) || ((in.i_mode & IFMT) != IFDIR)) {
    /* Not allocated or not a directory */
    return -1;
  }

  if (maxNumEntries < 1) return -1;
  int size = inode_getsize(&in);

  assert((size % sizeof(struct direntv6)) == 0);

  int count = 0;
  int numBlocks  = (size + DISKIMG_SECTOR_SIZE - 1) / DISKIMG_SECTOR_SIZE;
  char buf[DISKIMG_SECTOR_SIZE];
  struct direntv6 *dir = (struct direntv6 *) buf;
  for (int bno = 0; bno < numBlocks; bno++) {
    int bytesLeft, numEntriesInBlock, i;
    bytesLeft = file_getblock(fs, inumber,bno,dir);
    if (bytesLeft < 0) {
      printf("Error reading directory\n");
      return -1;
    }
    numEntriesInBlock = bytesLeft/sizeof(struct direntv6); 
    for (i = 0; i <  numEntriesInBlock ; i++) { 
      entries[count] = dir[i];
      count++;
      if (count >= maxNumEntries) return count;
    }
  }
  return count;
}

/* Function: dumpPathAndChildren
 * ----------------------------------
 * This function is taken from diskimageaccess.c.  It recursively prints out
 * the inodes and checksums for every file in the filesystem.  
 */
static void dumpPathAndChildren(const struct unixfilesystem *fs, const char *pathname, int inumber) {
  struct inode in;
  if (inode_iget(fs, inumber, &in) < 0) {
    printf("inode_iget(%d) returned < 0\n", inumber);
    return;
  }
  assert(in.i_mode & IALLOC);

  // Call and print result
  printf("\n");
  if (test_pathname_lookup_custom(fs, pathname) != inumber) {
    printf("\t->ERROR: expected return value of %d\n", inumber);
    return;
  };

  char chksum1[CHKSUMFILE_SIZE];
  int error_bno;
  int chksumresult = chksumfile_byinumber_error_checking(fs, inumber, chksum1, &error_bno);
  if (chksumresult < 0) {
    if (chksumresult == -1) {
      printf("\n\t->ERROR: Inode %d for path %s can't compute full file checksum; inode_iget(%d) returned < 0\n", inumber, pathname, inumber);
    } else if (chksumresult == -2) {
      printf("\n\t->ERROR: Inode %d for path %s can't compute full file checksum; file_getblock(%d, %d) returned < 0\n", inumber, pathname, inumber, error_bno);
    } else {
      printf("\n\t->ERROR: Inode %d for path %s can't compute full file checksum; checksum library error\n", inumber, pathname);
    }
    return;
  }

  char chksum2[CHKSUMFILE_SIZE];
  chksumresult = chksumfile_bypathname_error_checking(fs, pathname, chksum2, &error_bno);
  if (chksumresult < 0) {
    if (chksumresult == -1) {
      printf("\n\t->ERROR: Inode %d for path %s can't compute full file checksum; inode_iget(%d) returned < 0\n", inumber, pathname, inumber);
    } else if (chksumresult == -2) {
      printf("\n\t->ERROR: Inode %d for path %s can't compute full file checksum; file_getblock(%d, %d) returned < 0\n", inumber, pathname, inumber, error_bno);
    } else if (chksumresult == -3) {
      printf("\n\t->ERROR: Inode %d for path %s can't compute full file checksum; pathname_lookup(%s) returned < 0\n", inumber, pathname, pathname);
    } else {
      printf("\n\t->ERROR: Inode %d for path %s can't compute full file checksum; checksum library error\n", inumber, pathname);
    }
    return;
  }

  if (!chksumfile_compare(chksum1, chksum2)) {
    printf("Pathname checksum of %s differs from inode %d\n", pathname, inumber);
    printf("This usually means that the return value from pathname_lookup is incorrect,\n");
    printf("which causes the checksum to be calculated for the wrong inumber.\n");
    return;
  }

  char chksumstring[CHKSUMFILE_STRINGSIZE];
  chksumfile_cvt2string(chksum2, chksumstring);
  int size = inode_getsize(&in);
  printf("Path %s %d mode 0x%x size %d checksum %s\n",pathname,inumber,in.i_mode, size, chksumstring);

  if (pathname[1] == '\0') {
    /* pathame == "/" */
    pathname++; /* Delete extra / character */
  }

  // Recurse if directory
  if ((in.i_mode & IFMT) == IFDIR) { 
    const unsigned int MAXPATH = 1024;
    if (strlen(pathname) > MAXPATH-16) {
      printf("Too deep of directories %s\n", pathname);
    }

    struct direntv6 direntries[10000];
    int numentries = getDirEntries(fs, inumber, direntries, 10000);
    for (int i = 0; i < numentries; i++) {
      char *n =  direntries[i].d_name;
      if (n[0] == '.') {
        if ((n[1] == 0) || ((n[1] == '.') && (n[2] == 0))) {
            /* Skip over "." and ".." */
          continue;
        }
      }

        // Account for d_name not having null terminator
      char d_name[sizeof(direntries[i].d_name) + 1];
      strncpy(d_name, direntries[i].d_name, sizeof(direntries[i].d_name));
      d_name[sizeof(direntries[i].d_name)] = '\0';

      char nextpath[MAXPATH];
      sprintf(nextpath, "%s/%s",pathname, d_name);
      dumpPathAndChildren(fs, nextpath,  direntries[i].d_inumber);
    }
  }
}

/* Function: test_pathname_lookup
 * ----------------------------------
 * This function handles all testing for pathname_lookup; it expects one string
 * argument, which can be either "testX" (1 <= X <= 2) or an absolute path.
 *
 * If "test1": runs a test to look up a path that is not found
 * If "test2": runs a test for pathname_lookup on all files on the 
 *             specified disk
 *
 * Otherwise, it calls pathname_lookup with the specified path
 * and prints out the return value.
 */
static void test_pathname_lookup(const struct unixfilesystem *fs,
  const char *arg) {
  if (!strcmp(arg, "test1")) {
    printf("test1: look up an invalid absolute path\n\n");
    test_pathname_lookup_custom(fs, "/totallybogus");
  } else if (!strcmp(arg, "test2")) {
    printf("test2: printing pathname_lookup information for all files on this disk.\n\n");
    printf("Instead of printing all the block data for every block, this test prints a *checksum* value\n");
    printf("for the entire file's data.  A checksum is a single\n");
    printf("number representation of data that only matches if the data matches.  If the entire file checksum\n");
    printf("doesn't match, inspect the per-block checksums in lower layers for mismatches.  An incorrect pathname_lookup\n");
    printf("return value can also cause checksums to not match - check the output for any potential mismatches.\n\n");
    dumpPathAndChildren(fs, "/", ROOT_INUMBER);
  } else {
    // Custom test
    test_pathname_lookup_custom(fs, arg);
  }
}

static void printUsage(const char *progname) {
  printf("Usage: %s <options?> <diskimagePath> <function> <arg1>...<argn>\n\n", progname);
  printf("<options?> is optionally one of:\n");
  printf("-h               Print this message and exit.\n");
  printf("--help           Print this message and exit.\n");
  printf("--redirect-err   Redirect stderr to a file so it won't appear in\n");
  printf("                 program output; a generic message is printed if\n");
  printf("                 the file is non-empty after running the test(s).\n");
  printf("                 Used to check whether an error messsage is printed,\n");
  printf("                 without being sensitive to the exact message text.\n");
  printf("<diskimagePath> is the path to a disk image file\n");
  printf("                 (e.g. ones in samples/disk_images).\n");
  printf("<function> is one of the assignment functions, e.g.\n");
  printf("                 inode_indexlookup or file_getblock.\n");
  printf("<argX> specifies arguments to test that function.  You can run\n");
  printf("                 pre-provided tests or specify any arguments\n");
  printf("                 you'd like that are passed directly to the\n");
  printf("                 function to test its output.\n");
  printf("\n\nHere are arg options for each function you can test:\n\n");
  printf("inode_iget:\n");
  printf("                 - specify \"test1\" as arg to test inode_iget\n");
  printf("                   on all inodes on the disk\n");
  printf("                 - otherwise, specify the inode number to test\n");
  printf("inode_indexlookup:\n");
  printf("                 - specify \"test1\" as arg to test the first\n");
  printf("                   block of a small file\n");
  printf("                 - specify \"test2\" as arg to test the first\n");
  printf("                   block of a large file\n");
  printf("                 - specify \"test3\" as arg to test the last\n");
  printf("                   block of a doubly-indirect large file\n");
  printf("                 - specify \"test4\" as arg to test\n");
  printf("                   inode_indexlookup on all files on the disk\n");
  printf("                 - otherwise, specify the inode number followed\n");
  printf("                   by the fileBlockIndex to test with\n");
  printf("file_getblock:\n");
  printf("                 - specify \"test1\" as arg to test the first\n");
  printf("                   block of a small file\n");
  printf("                 - specify \"test2\" as arg to test the first\n");
  printf("                   block of a large file\n");
  printf("                 - specify \"test3\" as arg to test the last\n");
  printf("                   block of a doubly-indirect large file\n");
  printf("                 - specify \"test4\" as arg to test\n");
  printf("                   file_getblock on all files on the disk\n");
  printf("                 - otherwise, specify the inode number followed\n");
  printf("                   by the fileBlockIndex to test with\n");
  printf("directory_findname:\n");
  printf("                 - specify \"test1\" as arg to test a dirent\n");
  printf("                   that's not found in a partially-filled block\n");
  printf("                 - specify \"test2\" as arg to test a dirent\n");
  printf("                   with a length-14 name\n");
  printf("                 - specify \"test3\" as arg to test\n");
  printf("                   directory_findname on all files on the disk\n");
  printf("                 - otherwise, specify the directory inumber\n");
  printf("                   followed by the entry name to test with\n");
  printf("pathname_lookup:\n");
  printf("                 - specify \"test1\" as arg to test a path\n");
  printf("                   that's not found\n");
  printf("                 - specify \"test2\" as arg to test\n");
  printf("                   pathname_lookup on all files on the disk\n");
  printf("                 - otherwise, specify the absolute path\n");
  printf("                   to test with\n");
}

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    printf("Error: invalid parameters.\n");
    printUsage(argv[0]);
    return EXIT_FAILURE;
  }

  if ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)) {
    printUsage(argv[0]);
    return 0;
  }

  // redirect error messages to file
  bool quiet = false;
  if (strcmp(argv[1], "--redirect-err") == 0) {
    quiet = true;
    argv++;
    argc--;
  }

  if (argc < 4) {
    printf("Error: invalid parameters.\n");
    printUsage(argv[0]);
    return EXIT_FAILURE;
  }

  // First, load the specified disk image
  const char *diskpath = argv[1];
  int fd = diskimg_open(diskpath, 1);
  if (fd < 0) {
    printf("Can't open diskimagePath %s\n", diskpath);
    return EXIT_FAILURE;
  }
  struct unixfilesystem *fs = unixfilesystem_init(fd);
  if (!fs) {
    printf("Failed to initialize unix filesystem\n");
    return EXIT_FAILURE;
  }

  // Replace stderr with an output file
  int err_fd = -1;
  if (quiet) {
    close(STDERR_FILENO);
    err_fd = open("_stderr", O_RDWR | O_CREAT | O_TRUNC, 0664);
    if (err_fd < 0) {
      printf("Couldn't open \"_stderr\" for standard error output: %s\n",
        strerror(errno));
      return EXIT_FAILURE;
    }
    if (err_fd != 2) {
      printf("Failed to redirect stderr: opened file is fd %d\n", err_fd);
      return EXIT_FAILURE;
    }
  }

  bool error = false;

  // Next, identify which function is being tested
  if (strcmp(argv[2], "inode_iget") == 0) {
    test_inode_iget(fs, argv[3]);
  } else if (strcmp(argv[2], "inode_indexlookup") == 0) {
      test_inode_indexlookup(fs, argv + 3);
  } else if (strcmp(argv[2], "file_getblock") == 0) {
      test_file_getblock(fs, argv + 3);
  } else if (strcmp(argv[2], "directory_findname") == 0) {
      test_directory_findname(fs, argv + 3);
  } else if (strcmp(argv[2], "pathname_lookup") == 0) {
    test_pathname_lookup(fs, argv[3]);
  } else {
    printf("ERROR: unknown function '%s'.\n", argv[2]);
    error = true;
  }

  // Close the disk image when we're done
  int err = diskimg_close(fd);
  if (err < 0) {
    printf("Error closing %s\n", argv[1]);
  }
  free(fs);

  // Check if the error file has any output
  if (quiet) {
    struct stat stat;
    if (fstat(err_fd, &stat) != 0) {
      printf("fstat failed for stderr file: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    if (stat.st_size != 0) {
      printf("Error message(s) printed, written to temp file because of --redirect-err\n");
    }
    unlink("_stderr");
  }

  return error ? EXIT_FAILURE : 0;
}
