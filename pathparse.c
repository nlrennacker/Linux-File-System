/**************************************************************
 * Class:  CSC-415-03 Fall 2023
 * Names: Nathan Rennacker
 * Group Name: CN2S
 * Project: Basic File System
 *
 * File:
 *
 * Description: path parser for path sizes
 * 
 * 
 *
 **************************************************************/
#include "pathparse.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fsLow.h"
#include "mfs.h"

typedef struct {
    int location;
    int size;
} parsePathInfo;


static bool isAbsolutePath(const char *pathname) {
    return pathname[0] == '/';
}

static bool isSingleSlash(const char *pathname) {
    return strcmp(pathname, "/") == 0;
}

static bool isSingleDot(const char *pathname) {
    return strcmp(pathname, ".") == 0;
}

static bool isDoubleDot(const char *pathname) {
    return strcmp(pathname, "..") == 0;
}

/**
 * Sets the initial directory information based on the given pathname.
 *
 * @param dInfo Pointer to the parsePathInfo structure to be initialized.
 * @param pathname The path string used for initializing the structure.
 */
static void setInitialDirectoryInfo(parsePathInfo *dInfo, const char *pathname) {
    if (isAbsolutePath(pathname)) {
        dInfo->location = LBA_ROOT_LOC;
        dInfo->size = DE_SIZE * INIT_NUM_OF_DIRECT;
    } else {
        dInfo->location = curWorkingDir.directoryStartLocation;
        dInfo->size = curWorkingDir.d_reclen;
    }
}


/**
 * Retrieves the first token from the given pathname.
 *
 * @param pathname The path string to tokenize.
 * @param copy Pointer to the copied path string.
 * @param savePtr Pointer to the save pointer used by strtok_r.
 * @return The first token in the path string.
 */
static char *getFirstToken(const char *pathname, char **copy, char **savePtr) {
    char *token;
    *copy = strdup(pathname);

    if (strchr(pathname, '/') == NULL) {
        token = strdup(pathname);
        *savePtr = token + strlen(pathname); // Set savePtr to the end of the string
    } else {
        token = strtok_r(*copy, "/", savePtr);
    }

    return token;
}


/**
 * Handles special cases for pathnames (single slash, single dot, and double dot).
 *
 * @param pathname The path string to check for special cases.
 * @param dInfo Pointer to the parsePathInfo structure containing directory information.
 * @return A directory entry for the special case, or NULL if not a special case.
 */
static directoryEntry *handleSpecialCases(const char *pathname, parsePathInfo *dInfo) {
    directoryEntry *entry = malloc(sizeof(directoryEntry));

    if (isSingleSlash(pathname) || isSingleDot(pathname)) {
        directoryEntry *tempStructArray = malloc(MINBLOCKSIZE);
        LBAread(tempStructArray, 1, dInfo->location);
        memcpy(entry, &tempStructArray[0], sizeof(directoryEntry));
        free(tempStructArray);
    } else if (isDoubleDot(pathname)) {
        directoryEntry *tempCurrent = (directoryEntry *)malloc(ENTRIES_PER_BLOCK * MINBLOCKSIZE);
        LBAread(tempCurrent, 1, dInfo->location);
        directoryEntry *tempStructArray = malloc(DE_SIZE * MINBLOCKSIZE);
        LBAread(tempStructArray, 1, tempCurrent[1].location);
        memcpy(entry, &tempStructArray[0], sizeof(directoryEntry));
        free(tempStructArray);
        free(tempCurrent);
    } else {
        free(entry);
        return NULL;
    }

    return entry;
}

/**
 * Handles the ".." token, updating the parsePathInfo structure and last found entry.
 *
 * @param entryArray The array of directory entries.
 * @param dInfo Pointer to the parsePathInfo structure containing directory information.
 * @param lastFoundEntry Pointer to the last found directory entry.
 */
static void handleDotDotToken(directoryEntry *entryArray, parsePathInfo *dInfo, directoryEntry *lastFoundEntry) {
    dInfo->size = entryArray[1].fileSize;
    dInfo->location = entryArray[1].location;
    memcpy(lastFoundEntry, &entryArray[1], sizeof(directoryEntry));
}

/**
 * Searches for the given token in the array of directory entries, updating the parsePathInfo structure 
 * and last found entry if found.
 *
 * @param entryArray The array of directory entries.
 * @param token The token to search for in the directory entries.
 * @param dInfo Pointer to the parsePathInfo structure containing directory information.
 * @param lastFoundEntry Pointer to the last found directory entry.
 * @return true if the token is found in the directory entries, false otherwise.
 */
static bool findTokenInEntryArray(directoryEntry *entryArray, const char *token, parsePathInfo *dInfo, directoryEntry *lastFoundEntry) {
    int numberOfEntries = dInfo->size / DE_SIZE;

    for (int i = 0; i < numberOfEntries; i++) {
        if (!strcmp(entryArray[i].name, token)) {
            memcpy(lastFoundEntry, &entryArray[i], sizeof(directoryEntry));

            if (entryArray[i].isDirectory) {
                dInfo->location = entryArray[i].location;
                dInfo->size = entryArray[i].fileSize;
            }
            return true;
        }
    }

    return false;
}

directoryEntry *parsePath(const char *pathname) {
    
    parsePathInfo dInfo;
    setInitialDirectoryInfo(&dInfo, pathname);
    char *copy, *savePtr;
    char *token = getFirstToken(pathname, &copy, &savePtr);
    directoryEntry *lastFoundEntry = malloc(sizeof(directoryEntry));
    if (isSingleSlash(pathname) || isSingleDot(pathname) || isDoubleDot(pathname)) {
        lastFoundEntry = handleSpecialCases(pathname, &dInfo);
        free(copy);
        return lastFoundEntry;
    }
    while (token != NULL) {
        
        directoryEntry *entryArray = (directoryEntry *)malloc(dInfo.size);
        LBAread(entryArray, MIN_BLOCKS_PER_DIR, dInfo.location);
        
        if (isSingleDot(token)) {
            // do nothing
        } else if (isDoubleDot(token)) {
            handleDotDotToken(entryArray, &dInfo, lastFoundEntry);
        } else if (!findTokenInEntryArray(entryArray, token, &dInfo, lastFoundEntry)) {
            free(entryArray);
            free(lastFoundEntry);
            free(copy);
            return NULL;
        }
        token = strtok_r(NULL, "/", &savePtr);
        free(entryArray);
    }

    free(copy);
    return lastFoundEntry;
}