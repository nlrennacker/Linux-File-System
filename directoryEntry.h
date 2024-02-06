/**************************************************************
 * Class:  CSC-415-03 Fall 2023
 * Names:Caimin Rybolt-O’Keefe, Nathan Rennacker
 * Group Name: CN2S
 * Project: Basic File System
 *
 * File: directoryEntry.h
 *
 * Description: Header file for directory, includes exposed functions: initRootDirectory(), createDirectory
 * createEntry, readDirectory
 * 
 * 
 *
 **************************************************************/
#ifndef _DIRECTORY_H
#define _DIRECTORY_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "bitmap.h"

// Macros
#define INIT_NUM_OF_DIRECT 56 // Initial number of directory entries - 56
#define MAX_EXTENTS 3
#define LBA_ROOT_LOC 6
#define ENTRIES_PER_BLOCK 8
#define DE_SIZE 64
#define MIN_BLOCKS_PER_DIR 7

// Entry in directory
typedef struct directoryEntry {
    // Date entry was created
    // Known free state value: -1
    time_t date; // 8 bytes (time_t, assuming 64-bit)

    // Size of entry
    // Known free state value: 0
    uint32_t fileSize; // 4 bytes

    // Extents pointing to entry location in volume
    //      Extent:
    //          blockNumber - block location
    //          count - number of blocks
    extent extentLocations[MAX_EXTENTS]; // 12 bytes (assuming short blockNumber and count)

    // Location
    short location; // 2 bytes (short)

    // Whether entry is a directory
    // Known free state value: false
    bool isDirectory; // 1 byte (bool)

    // Name of entry
    // Known free state value: ""
    char name[37]; // 37 bytes
} directoryEntry;


/**
 * Initialize the root directory, and write to volume
 * @return block location of root directory in volume
*/
int initRootDirectory();

/**
 * Initialize group of directories, and write to volume
 * @param parentDir directoryEntry pointer to parent directory
 * @param newDirecName char pointer to name of the new Directory
 * @return block location of directory in volume
*/
int createDirectory(directoryEntry* parentDir, char* newDirecName);

/**
 * // Fill in entry info
 * @param entry directoryEntry pointer to entry to fill
 * @param name char pointer of name to of entry
 * @param isDirectory whether entry is a directory (true)
 * @param size size of entry
 * @param date date entry created
 * @param mapLocation location of entry
*/
void createEntry(directoryEntry* entry, char* name, bool isDirectory, uint32_t size, time_t date, int mapLocation);

/**
 * // Fill in entry info
 * @param entry directoryEntry pointer to entry to fill
 * @param name char pointer of name to of entry
 * @param isDirectory whether entry is a directory (true)
 * @param size size of entry
 * @param date date entry created
 * @param mapLocation location of entry
 * @param extents extent array pointer
*/
void copyEntry(directoryEntry* entry, char* name, bool isDirectory, uint32_t size, time_t date, int mapLocation, extent* extents);

/**
 * Read an entry back from the volume - used for debugging currently
*/
directoryEntry* readDirectory(int location, int blocks);

#endif // DIRECTORY_H
