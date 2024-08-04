#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>

#define ROOT_DIR_INUMBER 1

/* This function which looks up an absolute path (you can assume it is absolute,
   meaning starts with a "/") and returns the i-number for the file specified by 
   the path.
 */
int pathname_lookup(const struct unixfilesystem *fs, const char *pathname) {
    // cast pathname as char *
    size_t pathnameSize = strlen(pathname);
    char pathCopy[pathnameSize + 1];
    pathCopy[pathnameSize] = '\0';
    strncpy(pathCopy, pathname, pathnameSize);
    char *filepath = &pathCopy[0];

    // start at root node
    char delim[] = "/";
    char *dirname = strsep(&filepath, delim);
    int dirinumber = ROOT_DIR_INUMBER;
    while ((dirname = strsep(&filepath, delim)) != NULL) {
        struct direntv6 dirEnt;

        // edge case: if dirname is the empty string, return dirinumber
        if (strcmp(dirname, "") == 0) {
            return dirinumber;
        }

        if (directory_findname(fs, dirname, dirinumber, &dirEnt) == -1) {
            fprintf(stderr, "Invalid directory path. Name not found in directory.\n");
            return -1;
        }

        // update new dirinumber after finding next dirEnt
        dirinumber = dirEnt.d_inumber;
    }

    return dirinumber;
}
