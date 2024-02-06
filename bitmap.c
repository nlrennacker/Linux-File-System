/**************************************************************
 * Class:  CSC-415-03 Fall 2023
 * Names:Nathan Rennacker
 * Group Name: CN2S
 * Project: Basic File System
 *
 * File: bitmap.c
 *
 * Description: bitmap for managing free space The map's dimensions are 
 * based on the project's maximum size, which is 19,531 blocks of 512 bytes. 
 *
 *
 **************************************************************/
#include "bitmap.h"

#include <stdio.h>
#include <stdlib.h>

#include "fsLow.h"

/* FORWARD DECLARATION BLOCK */

/**
 * Set bits from the starting block location to the length.
 *
 * Returns -1 if the map blockSize is exceeded.
 * Returns 0 on success.
 */
int writeBlocks(int start, int length);

/**
 * TESTING CODE
 * prints the bitMap in form
 * m: 00000000000000000001111111111111
 * m: 00000000000000000000000000000000
 * reads RIGHT to LEFT
 */
void printMap();

/**
 * finds n empty bits in a row of length provided
 * returns block position if found
 * returns -1 if sequence cannot be found
 */
int findEmptyBlocks(int length, int start);

/**
 * Recursively count the empty blocks starting from the given bit.
 *
 * Returns n = length if all blocks were empty, otherwise returns n < length.
 */
int recursiveCount(int bit, int length);

// Set the specified bit within the map
void setBit(int bit);

// Clear the specified bit within the map
void clearBit(int bit);

// Find the specified bit within the map
int findBit(int bit);

/* FORWARD DECLARATION BLOCK END*/


int initMap(int lbaReadBool) {
    //block size 512 * 5 blocks = bytes
    bitmapPointer = calloc(1, (5 * MINBLOCKSIZE));
    if (bitmapPointer == NULL) {
        fprintf(stderr, "Memory Allocation Error");
        return -1;
    }
    //if reading from the LBA
    if (lbaReadBool) {
        LBAread(bitmapPointer,5,1);

    //otherwise zero bitmap memory and allocate space for self
    } else {
        for (int i = 0; i < NUM_BLOCKS - 1; i++) {
            clearBit(i);
        }
        //writing 5 blocks for bitmap's own memory
        writeBlocks(1, 5);
        LBAwrite(bitmapPointer, 5, 1);
    }

    return 1;
}

int freeMap() {
    if (bitmapPointer != NULL) {
        free(bitmapPointer);
        bitmapPointer = NULL;
        return 0;
    }
    // invalid pointer
    return -1;
}

int allocateFirstBlocks(int length) {
    int blockPos = findEmptyBlocks(length, 0);
    if (blockPos < 0) {
        fprintf(stderr, "ERROR: no free space found");
        return -1;
    }

    writeBlocks(blockPos, length);
    LBAwrite(bitmapPointer, 5, 1);
    return blockPos;
}

int allocateAdditionalBlocks(int location, int initialSize, int additionalSize, extent* extentArray) {
    // TODO
    // Finish comments for this function

    //check if extents have non-zero count
    int lastNonZeroIndex = -1;
    int usingExtents = 0;
    for (int i = 2; i >= 0; --i) {
        if (extentArray[i].count != 0) {
            lastNonZeroIndex = i;
            break;
        }
    }

    // Set startLocation to the block thats either a non-zero index from an extent or the location of the end of the regular allocated block
    int startLocation = (lastNonZeroIndex != -1) ? ((int)extentArray[lastNonZeroIndex].blockNumber + extentArray[lastNonZeroIndex].count) : (location + initialSize);

    //
    int blockPos = findEmptyBlocks(additionalSize, startLocation);
    if (blockPos == startLocation) {
        if (lastNonZeroIndex != -1) {
            extentArray[lastNonZeroIndex].count += additionalSize;
        }
    } else {
        blockPos = findEmptyBlocks(additionalSize, 0);
        if (blockPos < 0) {
            fprintf(stderr, "ERROR: no free space found");
            return -1;
        }
        usingExtents = 1;
        extentArray[lastNonZeroIndex + 1].blockNumber = blockPos;
        extentArray[lastNonZeroIndex + 1].count = additionalSize;
    }
    if (blockPos < 0) {
        fprintf(stderr, "ERROR: no free space found");
        return -1;
    }
    writeBlocks(blockPos, additionalSize);
    LBAwrite(bitmapPointer, 5, 1);
    return usingExtents;
}

int writeBlocks(int start, int length) {
    if (start + length > NUM_BLOCKS) {
        printf("Trying to set bits exceeding number of blocks");
        return -1;
    }
    for (int i = start; i < (start + length); i++) {
        setBit(i);
    }
    return 0;
}

int clearBlocks(int start, int length) {
    if (start + length > NUM_BLOCKS) {
        fprintf(stderr, "Trying to clear bits exceeding number of blocks\n");

        return -1;
    }
    // clears the bits in the initial location
    for (int i = start; i < (start + length); i++) {
        clearBit(i);
    }
    LBAwrite(bitmapPointer, 5, 1);
    return 0;
}

int findEmptyBlocks(int length, int start) {
    int i = start;
    while (i < NUM_BLOCKS - 1) {
        if (findBit(i) == 0) {
            int foundLength = recursiveCount(i, length);
            if (foundLength == length) {
                return i;
            } else {
                // Skip the checked bits
                i += (foundLength > 0) ? foundLength : 1;
            }
        } else {
            i++;
        }
    }
    return -1;
}

int recursiveCount(int bit, int length) {
    if (length == 0) {
        return 0;
    } else if (findBit(bit) == 0) {
        return recursiveCount((bit + 1), length - 1) + 1;
    } else {
        // Return the number of consecutive empty bits found
        return length - 1;
    }
}

void setBit(int bit) {
    // takes the value from the map (with the correct offset for integer)
    // and uses an OR operation to set a specific bit (that corresponds to a block)
    // << is a LEFT SHIFT operation effectively multiplying bit * 2^1
    bitmapPointer->map[INT_OFFSET(bit)] |= ((uint32_t)1 << BIT_OFFSET(bit));
}

void clearBit(int bit) {
    // same as above but uses an AND operation (and one's complement) to clear the bit
    bitmapPointer->map[INT_OFFSET(bit)] &= ~((uint32_t)1 << BIT_OFFSET(bit));
}

int findBit(int bit) {
    // checks to see if the specific block in the bit is set
    if ((bitmapPointer->map[INT_OFFSET(bit)] & ((uint32_t)1 << BIT_OFFSET(bit)))) {
        return 1;
    } else {
        return 0;
    }
}

void printMap() {
    for (int m = 0; m < (NUM_BLOCKS / 32); m++) {
        printf("m: " PRINTF_BINARY_PATTERN_INT32 "\n",
               PRINTF_BYTE_TO_BINARY_INT32(bitmapPointer->map[m]));
    }
    printf("\n");
}
