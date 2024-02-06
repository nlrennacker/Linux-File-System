/**************************************************************
 * Class:  CSC-415-03 Fall 2023
 * Names:Nathan Rennacker
 * Group Name: CN2S
 * Project: Basic File System
 *
 * File: bitmap.h
 *
 * Description: Header file for the bitmap, includes exposed functions: initMap(), freeMap(), 
 * allocateFirstBlocks(), allocateAdditionalBlocks, and the structure 
 * 
 * 
 *
 **************************************************************/
#ifndef _BITMAP_H
#define _BITMAP_H

#include <stdint.h>

#define NUM_BLOCKS 19531

// Calculate the number of bits in a uint32_t
#define BITS_PER_UINT (sizeof(uint32_t) * 8)

// Convert block number for bitwise operations:
// Calculate which integer within the bitMap array corresponds to the block
#define INT_OFFSET(b) ((b) / BITS_PER_UINT)

// uses remainder (modulo) to get which specific bit to work on within 32
// ie we want to get the 1542nd block -> 1542 % 32 = 12, so this means we want the 12th bit within the 32
#define BIT_OFFSET(b) ((b) % BITS_PER_UINT)


/* --- testing code for printing bitmap --- */

#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64    \
    PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)

/* --- end print macros --- */


typedef struct bitmap {
    // Bitmap stores 19531 bits, represented by 611 uint32_t values
    uint32_t map[(NUM_BLOCKS/32) + 1]; 
} bitmap;

typedef struct extent{
    short blockNumber;  // block start location
    short count;  // length of the entry
}extent;

bitmap* bitmapPointer;

/**
 * Initializes the block bitmap and reserves the first five blocks for it.
 * Allocates memory for the bitmap and sets all bits to 0, indicating that blocks are free.
 * Writes the bitmap to the LBA if not already existing
 * 
 * @param lbaReadBool 0 if bitmap does not exist in LBA, 1 if it does and should be read from the LBA
 *
 * @return The location of the bitmap in the LBA, -1 on memory allocation error.
 */
int initMap(int lbaReadBool);

/**
 * Frees the memory allocated for the block bitmap and sets the pointer to NULL.
 *
 * @return 0 if the memory was successfully freed, -1 if the pointer was already NULL or invalid.
 */
int freeMap();

/**
 * Allocates a contiguous sequence of new blocks of the specified length.
 * Writes the allocated blocks to the bitmap and writes to the LBA.
 *
 * @param length The number of contiguous blocks to be allocated.
 *
 * @return The starting block position of the allocated blocks, or -1 if no free space is found.
 */
int allocateFirstBlocks( int length );

/**
 * Allocates additional contiguous blocks to extend a previously allocated set of blocks.
 * If the additional blocks cannot be found right after the existing blocks, a new extent is used.
 * If extents are needed, the extent array is updated with the new extent's starting block number and count of blocks.
 * Finally, writes to the LBA
 *
 * @param location      The starting block number of the previously allocated set of blocks.
 * @param initialSize   The initial size of the previously allocated set of blocks (non-extent size).
 * @param additionalSize The number of additional blocks needed.
 * @param extentArray   A pointer to an extent array that will be updated with the new extent.
 *                      The array should have at least 3 elements.
 *
 * @return 0 if the additional blocks are allocated without using extents, 1 otherwise.
 * The returned value is not always an error code but function will return -1 if allocation fails.
 */
int allocateAdditionalBlocks(int location, int initialSize, int additionalSize, extent extentArray[3]);

/**
 * Clears the specified number of blocks in the bitmap, starting from the given block index.
 * Writes the updated map to the LBA
 *
 * @param start The starting block index to clear.
 * @param length The number of blocks to clear.
 * @return 0 on success, -1 if the block range exceeds the total number of blocks.
 */
int clearBlocks(int start, int length);


#endif