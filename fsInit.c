/**************************************************************
 * Class:  CSC-415-03 - Spring 2023
 * Names: Nathan Rennacker, Suzanna Li 
 * Group Name: CN2S
 * Project: Basic File System
 *
 * File: fsInit.c
 *
 * Description: Main driver for file system assignment.
 *
 * This file is where you will start and initialize your system
 *
 **************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "directoryEntry.h"
#include "bitmap.h"
#include "fsLow.h"
#include "mfs.h"

typedef struct volumeControlBlock {
    int totalBlock;    // total number of blocks
    int freeBlock;     // number of free blocks
    int blockSize;     // size= 512
    int rootLocation;  // location of root
    int mapLocation;   // location of free space map
    int initNumber;    // the numbe to check if VCB initilized
} VCB;

VCB* vcbPointer;
int magicNumber = 41804519;  // check if volume is initilized


int initVolumeControl(uint64_t numBlock, uint64_t bSize) {
    int bitmapLocation = initMap(0);
    int vcbLocation = allocateFirstBlocks(1);
    // check if bitmap is initilized & valid
    if (bitmapLocation == -1) {
        printf("Error in initilizing bitmap.\n");
        return -1;
    }

    // writing Block 0
    vcbPointer->totalBlock = numBlock;
    vcbPointer->blockSize = bSize;
    vcbPointer->freeBlock = 0;
    vcbPointer->initNumber = magicNumber;
    vcbPointer->mapLocation = bitmapLocation;
    vcbPointer->rootLocation = initRootDirectory();

    LBAwrite(vcbPointer, 1, vcbLocation);

    return 0;
}

int initFileSystem(uint64_t numberOfBlocks, uint64_t blockSize) {
    printf("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);
    vcbPointer = malloc(sizeof(VCB));

    char * tempBuf = malloc(blockSize);
    LBAread(tempBuf, 1, 0);
    memcpy(vcbPointer, tempBuf, sizeof(VCB));
    free(tempBuf);

    if (vcbPointer->initNumber != magicNumber) {
        initVolumeControl(numberOfBlocks, blockSize);
    } else {
        initMap(1);
    }

    return 0;
}


void exitFileSystem() {
    printf("System exiting\n");
    free(vcbPointer);
    freeMap();
}
