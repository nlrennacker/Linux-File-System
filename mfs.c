/**************************************************************
 * Class:  CSC-415-03 Fall 2023
 * Names: Nathan Rennacker, Suzanna Li
 * Group Name: CN2S
 * Project: Basic File System
 *
 * File: 
 *
 * Description:  file functions
 * 
 * 
 *
 **************************************************************/
#include "mfs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsLow.h"
#include "pathparse.h"

// cwd is initialized to root at start
fdDir curWorkingDir = {.d_reclen = DE_SIZE * INIT_NUM_OF_DIRECT, .dirEntryPosition = 0, .directoryStartLocation = LBA_ROOT_LOC};

int fs_mkdir(const char *pathname, mode_t mode) {
    // Check if the directory already exists
    directoryEntry *entry = parsePath(pathname);
    if (entry != NULL) {
        return 0;
    }

    // Make a copy of the pathname
    char temp[PATH_MAX];
    strncpy(temp, pathname, PATH_MAX - 1);
    temp[PATH_MAX - 1] = '\0';

    // Find the last slash in the pathname
    char *last_slash = strrchr(temp, '/');

    if (last_slash != NULL) {
        // Extract the new directory name from the pathname
        char part[PATH_MAX];
        strncpy(part, last_slash + 1, PATH_MAX - 1);
        part[PATH_MAX - 1] = '\0';

        // Remove the new directory name from the pathname
        *last_slash = '\0';

        // Ensure the pathname starts with a slash
        if (temp[0] == '\0') {
            temp[0] = '/';
            temp[1] = '\0';
        }

        // Check if the parent directory exists
        entry = parsePath(temp);
        if (entry == NULL) {
            fprintf(stderr, "ERROR: path not found\n");
            return -1;
        }

        // Create the new directory in the parent directory
        createDirectory(entry, part);
    } else {
        // If there's no slash, create the new directory in the current directory
        entry = parsePath(".");
        createDirectory(entry, temp);
    }
    return 0;
}

int fs_rmdir(const char *pathname) {
    directoryEntry *entry = parsePath(pathname);
    if (entry == NULL || !entry->isDirectory) {
        return -1;
    }

    directoryEntry *direcToDelete = (directoryEntry *)malloc(entry->fileSize);
    LBAread(direcToDelete, MIN_BLOCKS_PER_DIR, entry->location);

    // check if direct to delete is empty
    for (int i = 0; i < INIT_NUM_OF_DIRECT; i++) {
        if (strcmp(direcToDelete[i].name, ".") == 0 || strcmp(direcToDelete[i].name, "..") == 0) {
            continue;
        } else if (direcToDelete[i].date != -1) {  // check if entryarray is same name as file name
            free(entry);
            free(direcToDelete);
            fprintf(stderr, "Directory not empty.\n");
            return -2;
        }
    }

    // delete entry in parent
    directoryEntry *parentDirec = (directoryEntry *)malloc(direcToDelete[1].fileSize);
    LBAread(parentDirec, MIN_BLOCKS_PER_DIR, direcToDelete[1].location);

    for (int i = 0; i < INIT_NUM_OF_DIRECT; i++) {
        if (strcmp(parentDirec[i].name, entry->name) == 0) {  // check if entryarray is same name as file name
            // clear blocks from bitmap first
            clearBlocks(parentDirec[i].location, (parentDirec[i].fileSize + MINBLOCKSIZE - 1) / MINBLOCKSIZE);
            strcpy(parentDirec[i].name, "");
            parentDirec[i].fileSize = 0;
            parentDirec[i].isDirectory = false;
            parentDirec[i].location = -1;
            parentDirec[i].date = -1;
            break;
        }
    }

    LBAwrite(parentDirec, MIN_BLOCKS_PER_DIR, direcToDelete[1].location);
    free(entry);
    free(direcToDelete);
    free(parentDirec);
    return 0;
    // removes each directory on the path if and only if it is empty
}

// Directory iteration functions
fdDir *fs_opendir(const char *pathname) {
    directoryEntry *entry = parsePath(pathname);
    if (entry == NULL) {
        fprintf(stderr, "ERROR: path not found\n");
        return NULL;
    }

    fdDir *dir = malloc(sizeof(fdDir));
    dir->d_reclen = entry->fileSize;
    dir->directoryStartLocation = entry->location;
    dir->dirEntryPosition = 0;
    free(entry);
    return dir;
}

struct fs_diriteminfo *fs_readdir(fdDir *dirp) {
    directoryEntry *entryArray = (directoryEntry *)malloc(dirp->d_reclen);
    LBAread(entryArray, dirp->d_reclen / MINBLOCKSIZE, dirp->directoryStartLocation);

    struct fs_diriteminfo *dirItemInfo = malloc(sizeof(struct fs_diriteminfo));

    do {
        dirItemInfo->d_reclen = entryArray[dirp->dirEntryPosition].fileSize;
        dirItemInfo->fileType = entryArray[dirp->dirEntryPosition].isDirectory ? 4 : 8;
        strncpy(dirItemInfo->d_name, entryArray[dirp->dirEntryPosition].name, sizeof(dirItemInfo->d_name));

        dirp->dirEntryPosition++;

    } while (strcmp(dirItemInfo->d_name, "") == 0 && dirp->dirEntryPosition < INIT_NUM_OF_DIRECT);

    free(entryArray);

    if (strcmp(dirItemInfo->d_name, "") == 0) {
        free(dirItemInfo);
        return NULL;
    }

    return dirItemInfo;
}

int fs_closedir(fdDir *dirp) {
    free(dirp);
    dirp = NULL;
    return 0;
}

// Misc directory functions
char *fs_getcwd(char *pathname, size_t size) {
    // Temporary variable to store the current directory
    directoryEntry *cwd = (directoryEntry *)malloc(INIT_NUM_OF_DIRECT * DE_SIZE);
    LBAread(cwd, MIN_BLOCKS_PER_DIR, curWorkingDir.directoryStartLocation);

    // Initialize pathname with an empty string
    pathname[0] = '\0';

    int prevLocation = curWorkingDir.directoryStartLocation;
    // While the current directory is not the root directory
    while (cwd[0].location != LBA_ROOT_LOC) {
        int parentLocation = cwd[1].location;  // Store the parent directory location
        LBAread(cwd, MIN_BLOCKS_PER_DIR, parentLocation);

        for (int i = 0; i < DE_SIZE; i++) {
            directoryEntry *entry = &cwd[i];

            // Find the entry that points to the current directory
            if (entry->location == prevLocation && strcmp(entry->name, ".") != 0) {
                char temp[PATH_MAX];
                snprintf(temp, sizeof(temp), "/%s%s", entry->name, pathname);
                strncpy(pathname, temp, size - 1);
                pathname[size - 1] = '\0';
                break;
            }
        }

        prevLocation = parentLocation;
    }

    // Prepend a '/' to the pathname string if it's not already there
    if (strlen(pathname) == 0 || pathname[0] != '/') {
        char temp[PATH_MAX];
        snprintf(temp, sizeof(temp), "/%s", pathname);
        strncpy(pathname, temp, size - 1);
        pathname[size - 1] = '\0';
    }

    free(cwd);
    return pathname;
}

int fs_setcwd(char *pathname) {
    directoryEntry *entry = parsePath(pathname);
    if (entry == NULL) {
        fprintf(stderr, "ERROR: path not found\n");
        return -1;
    }
    if (!entry->isDirectory) {
        fprintf(stderr, "%s: not a directory\n", entry->name);
        return -2;
    }

    curWorkingDir.d_reclen = entry->fileSize;
    curWorkingDir.dirEntryPosition = 0;
    curWorkingDir.directoryStartLocation = entry->location;

    free(entry);
    return 0;
}

int fs_isFile(char *filename) {  // return 1 if file, 0 otherwise
    // if all this is given is a filename, do we have to traverse the entire file system and look for this specific file?
    directoryEntry *entry = parsePath(filename);
    if (entry == NULL) {
        return -1;
    }
    if (!entry->isDirectory) {
        return 1;
    } else {
        return 0;
    }
    free(entry);
}
// return 1 if file, 0 otherwise
int fs_isDir(char *pathname) {
    directoryEntry *entry = parsePath(pathname);
    if (entry == NULL) {
        return -1;
    }
    if (entry->isDirectory) {
        return 1;
    } else {
        return 0;
    }
    free(entry);
}

int fs_delete(char *filename) {  // removes file
    // only looks in current directory
    directoryEntry *entryArray = (directoryEntry *)malloc(curWorkingDir.d_reclen);
    LBAread(entryArray, MIN_BLOCKS_PER_DIR, curWorkingDir.directoryStartLocation);

    // loop through the array    //size of the direcotry / blocksize of blocks
    for (int i = 0; i < curWorkingDir.d_reclen / MINBLOCKSIZE; i++) {
        if (strcmp(entryArray[i].name, filename) == 0) {  // check if entryarray is same name as file name
            // clear blocks from bitmap first
            clearBlocks(entryArray[i].location, ceil(entryArray[i].fileSize / MINBLOCKSIZE));
            strcpy(entryArray[i].name, "");
            entryArray[i].fileSize = 0;
            entryArray[i].isDirectory = false;
            entryArray[i].location = -1;
            entryArray[i].date = -1;

            // free bitmap please

            break;
        }
    }

    // write to LBA to update the deletion of the file
    LBAwrite(entryArray, curWorkingDir.d_reclen / MINBLOCKSIZE, curWorkingDir.directoryStartLocation);
}

void concatPath(char *dest, const char *src) {
    char *lastToken = strrchr(src, '/');
    if (lastToken != NULL) {
        lastToken++;  // Move the pointer after the last '/'
    } else {
        lastToken = (char *)src;  // If there is no '/', the entire path is the last token
    }
    // Ensure there is a '/' at the end of the destPath if not present
    if (dest[strlen(dest) - 1] != '/') {
        strcat(dest, "/");
    }

    // Concatenate the last token to the destPath
    strcat(dest, lastToken);
}

int checkSrcInDestPath(const char *srcPath, const char *destPath) {
    int srcLen = strlen(srcPath);
    int destLen = strlen(destPath);

    if (destLen <= srcLen) {
        return 0;
    }

    if (strncmp(srcPath, destPath, srcLen) == 0 && (destPath[srcLen] == '/' || destPath[srcLen] == '\0')) {
        return 1;
    }

    return 0;
}

int fs_move(const char *srcPathname, const char *destPathname) {
    directoryEntry *srcEntry = parsePath(srcPathname);
    directoryEntry *destEntry = parsePath(destPathname);

    if (srcEntry == NULL) {
        free(destEntry);
        free(srcEntry);
        fprintf(stderr, "Source doesn't exist\n");
        return -1;
    }
    if (destEntry == NULL) {
        free(destEntry);
        free(srcEntry);
        fprintf(stderr, "Destination folder doesn't exist\n");
        return -2;
    }
    if (!destEntry->isDirectory) {
        free(destEntry);
        free(srcEntry);
        fprintf(stderr, "Destination is a file\n");
        return -3;
    }
    if (checkSrcInDestPath(srcPathname, destPathname)) {
        free(destEntry);
        free(srcEntry);
        fprintf(stderr, "Cannot move a directory into its own subdirectory\n");
        return -5;
    }
    directoryEntry *selfDirect = NULL;
    char temp[PATH_MAX];
    if (!srcEntry->isDirectory) {
        strncpy(temp, srcPathname, PATH_MAX - 1);
        temp[PATH_MAX - 1] = '\0';

        // Find the last slash in the pathname
        char *last_slash = strrchr(temp, '/');
        if (last_slash != NULL) {
            char part[PATH_MAX];
            strncpy(part, last_slash + 1, PATH_MAX - 1);
            part[PATH_MAX - 1] = '\0';

            // Remove the new directory name from the pathname
            *last_slash = '\0';

            // Ensure the pathname starts with a slash
            if (temp[0] == '\0') {
                temp[0] = '/';
                temp[1] = '\0';
            }
            selfDirect = parsePath(temp);
        } else {
            selfDirect = parsePath(".");
        }
    }

    char destDup[PATH_MAX];
    strncpy(destDup, destPathname, sizeof(destDup) - 1);
    destDup[sizeof(destDup) - 1] = '\0';
    concatPath(destDup, srcPathname);

    directoryEntry *testDest = parsePath(destDup);
    if (testDest != NULL) {
        fprintf(stderr, "Destination name already exists\n");
        return -4;
    }
    free(testDest);
    // destination directory
    directoryEntry *destDirect = (directoryEntry *)malloc(INIT_NUM_OF_DIRECT * DE_SIZE);
    LBAread(destDirect, MIN_BLOCKS_PER_DIR, destEntry->location);

    // directory being moved
    directoryEntry *movedDirectory = (directoryEntry *)malloc(INIT_NUM_OF_DIRECT * DE_SIZE);
    directoryEntry *srcDirect = (directoryEntry *)malloc(INIT_NUM_OF_DIRECT * DE_SIZE);
    if (selfDirect == NULL) {
        LBAread(movedDirectory, 1, srcEntry->location);
        LBAread(srcDirect, MIN_BLOCKS_PER_DIR, movedDirectory[1].location);
    } else {
        LBAread(movedDirectory, 1, selfDirect->location);
        LBAread(srcDirect, MIN_BLOCKS_PER_DIR, movedDirectory[0].location);
    }

    // copy directory
    for (int i = 0; i < INIT_NUM_OF_DIRECT; i++) {
        if (destDirect[i].date == -1) {
            memcpy(&destDirect[i], srcEntry, sizeof(directoryEntry));
            if (srcEntry->isDirectory) {
                movedDirectory[1].location = destDirect[0].location;
            }
            break;
        }
    }

    LBAwrite(destDirect, MIN_BLOCKS_PER_DIR, destEntry->location);
    LBAwrite(movedDirectory, 1, movedDirectory[0].location);
    free(movedDirectory);
    free(destDirect);

    directoryEntry *blankEntry = calloc(1, sizeof(directoryEntry));
    createEntry(blankEntry, "", false, 0, -1, -1);

    // deleting source
    for (int i = 0; i < INIT_NUM_OF_DIRECT; i++) {
        if (strcmp(srcDirect[i].name, srcEntry->name) == 0) {
            memcpy(&srcDirect[i], blankEntry, sizeof(directoryEntry));
            break;
        }
    }
    LBAwrite(srcDirect, MIN_BLOCKS_PER_DIR, srcDirect[0].location);

    if (selfDirect != NULL) {
        free(selfDirect);
    }
    free(srcDirect);
    free(blankEntry);
    free(destEntry);
    free(srcEntry);
    return 0;
}

int fs_stat(const char *path, struct fs_stat *buf) {
    directoryEntry *entry = parsePath(path);

    // If entry does not exist
    if (entry == NULL)
        return -1;

    // Entry block size
    buf->st_blksize = 512;

    // Entry file size info
    buf->st_blocks = (entry->fileSize + MINBLOCKSIZE - 1) / MINBLOCKSIZE;
    buf->st_size = entry->fileSize;

    // Entry dates
    buf->st_createtime = entry->date;
    buf->st_accesstime = 0;
    buf->st_modtime = 0;

    // Entry start block location
    buf->st_location = entry->location;
    // Entry extent info
    memcpy(&(buf->st_extents), &(entry->extentLocations), sizeof(extent) * MAX_EXTENTS);

    strcpy(buf->st_name, entry->name);
    buf->st_name[PATH_MAX - 1] = '\0';

    free(entry);

    return 0;
}