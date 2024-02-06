/**************************************************************
 * Class:  CSC-415-03 Fall 2023
 * Names:Caimin Rybolt-O’Keefe, Nathan Rennacker
 * Group Name: CN2S
 * Project: Basic File System
 *
 * File: directoryEntry.c
 *
 * Description: Main file for creating directory entries
 * initializing root directory and other directory related operations 
 *
 *
 **************************************************************/
#include "directoryEntry.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include "fsLow.h"

// Function Implementations
void createEntry(directoryEntry* entry, char* name, bool isDirectory, uint32_t size, time_t date, int mapLocation) {
	strcpy(entry->name, name);
	entry->isDirectory = isDirectory;
	entry->fileSize = size;
	entry->date = date;
	entry->location = mapLocation;

	for (int j = 0; j < MAX_EXTENTS; j++) {
		extent * ext = &(entry->extentLocations[j]);
		ext->blockNumber = 0;
		ext->count = 0;
	}
}

void copyEntry(directoryEntry* entry, char* name, bool isDirectory, uint32_t size, time_t date, int mapLocation, extent* extents) {
	strcpy(entry->name, name);
	entry->isDirectory = isDirectory;
	entry->fileSize = size;
	entry->date = date;
	entry->location = mapLocation;
	
	memcpy(entry->extentLocations, extents, sizeof(extent) * MAX_EXTENTS);
}


int createDirectory(directoryEntry* parentDir, char* newDirecName) {
	// Buffer for reading/writing a block of directory entries (DEs)
	directoryEntry * buffBlockDE = (directoryEntry *)calloc(ENTRIES_PER_BLOCK, DE_SIZE);

	/// CHECK FOR EXISTING DE WITH name FROM newDirecName
	///		- side objective: find first blankDE to fill
	// Info for blank DE to fill - if found while searching parent for duplicate name
	int blankDE_block = -1;
	int blankDE_index = -1;

	// Loop through parent + extents (if exist)
	for (int i = -1; i < MAX_EXTENTS; i++) {
		int blockLocation = 0;	// location in volume to read from
		
		if (i == -1)	// block location is parent location
			blockLocation = parentDir->location;
		else			// OR one of parent's extent locations
			blockLocation = parentDir->extentLocations[i].blockNumber;

		if (blockLocation <= 0)
			continue;

		// Loop through each DE
		for (int j = 0; j < INIT_NUM_OF_DIRECT; j++) {
			// Read 1 block into buffer array every 8 DE (at block location + offset), including 0
			if (j % ENTRIES_PER_BLOCK == 0)
				LBAread(buffBlockDE, 1, blockLocation + (j / ENTRIES_PER_BLOCK));

			directoryEntry * currEntry = &(buffBlockDE[j % ENTRIES_PER_BLOCK]);

			// If duplicate DE name found, exit with -1
			if (!strcmp(currEntry->name, newDirecName)) {
				free(buffBlockDE);
				return -1;
			}

			// If blank DE found (when location == -1)
			// AND a blank DE not found yet
			// THEN note current extent and DE index of that extent
			//		ext = -1 if initial volume of parent DEs
			if (blankDE_index < 0 && currEntry->date == -1) {
				blankDE_block = blockLocation + (j / ENTRIES_PER_BLOCK);
				blankDE_index = j % ENTRIES_PER_BLOCK;
			}
		}
	}

	// If no blank DE found, find a free extent to allocate towards and use that first DE
	if (blankDE_index < 0) {
		int freeExtent = -1;

		// Find free extent
		for (int i = 0; i < MAX_EXTENTS; i++) {
			if (parentDir->extentLocations[i].blockNumber <= 0) {
				freeExtent = i;
				break;
			}
		}
		// If no free extents found, exit with -2
		if (freeExtent < 0) {
			free(buffBlockDE);
			return -2;
		}
		
		// Allocate new blocks
		int alloLoc =  allocateFirstBlocks(INIT_NUM_OF_DIRECT / ENTRIES_PER_BLOCK);
		// If no blocks can be allocated, exit with -3
		if (alloLoc < 0) {
			free(buffBlockDE);
			return -3;
		}
		
		// Update newly allocated extent for parent
		parentDir->extentLocations[freeExtent].blockNumber = alloLoc;
		parentDir->extentLocations[freeExtent].count = INIT_NUM_OF_DIRECT / ENTRIES_PER_BLOCK;

		// Fill every DE of the newly allocated extent with free DEs
		directoryEntry * tempBuffer = (directoryEntry *)calloc(ENTRIES_PER_BLOCK, DE_SIZE);
		for (int i = 0; i < INIT_NUM_OF_DIRECT; i++) {
			createEntry(&(tempBuffer[i % ENTRIES_PER_BLOCK]), "", false, 0, -1, -1);
			
			// Write to volume every 8 entries
			if (i % ENTRIES_PER_BLOCK == ENTRIES_PER_BLOCK - 1)
				LBAwrite(tempBuffer, 1, alloLoc + (i / ENTRIES_PER_BLOCK));
		}

		// Update parent's size
		LBAread(tempBuffer, 1, parentDir->location);
		tempBuffer[0].fileSize += INIT_NUM_OF_DIRECT * DE_SIZE;
		LBAwrite(tempBuffer, 1, parentDir->location);
		parentDir->fileSize += INIT_NUM_OF_DIRECT * DE_SIZE;

		free(tempBuffer);

		blankDE_block = alloLoc;
		blankDE_index = 0;
	}

	/// CHECK FOR USEABLE FREE BLOCKS
	// Allocate blocks for new DE: if no blocks available, exit with -2
	int mapLocation = allocateFirstBlocks(INIT_NUM_OF_DIRECT / ENTRIES_PER_BLOCK);
	if (mapLocation == -1) {
		free(buffBlockDE);
		return -2;
	}

	for (int i = 0; i < INIT_NUM_OF_DIRECT; i++) {
		directoryEntry * currEntry = &(buffBlockDE[i % ENTRIES_PER_BLOCK]);

		// create DE for Directory we are creating (selfDE)
		if (i == 0) {
			createEntry(currEntry, ".", true, DE_SIZE * INIT_NUM_OF_DIRECT, time(0), mapLocation);

			// Read from volume to temp buffer the block a free DE was found at
			//		AKA the block to edit
			directoryEntry * tempBuffer = (directoryEntry *)calloc(ENTRIES_PER_BLOCK, DE_SIZE);
			LBAread(tempBuffer, 1, blankDE_block);

			// Copy memory of selfDE to the entry in block
			memcpy(&(tempBuffer[blankDE_index]), currEntry, DE_SIZE);
			strncpy(tempBuffer[blankDE_index].name, newDirecName, sizeof(currEntry->name));	// but give it the new name

			// Write to volume the updated block
			LBAwrite(tempBuffer, 1, blankDE_block);

			free(tempBuffer);	// free the temp buffer
		}
		// create DE for parent directory
		else if (i == 1) {
			memcpy(currEntry, parentDir, DE_SIZE);
			strncpy(currEntry->name, "..", sizeof(currEntry->name));
		}
		// create DE in a known-free state
		// 		(date=-1, name="", fileSize=0, isDirectory=false, extents={0,0})
		else
			createEntry(currEntry, "", false, 0, -1, -1);
		
		// Write buffer to volume at end of every (ENTRIES_PER_BLOCK)th iteration
		if (i % ENTRIES_PER_BLOCK == ENTRIES_PER_BLOCK - 1)
			LBAwrite(buffBlockDE, 1, mapLocation + (i / ENTRIES_PER_BLOCK));
	}

	free(buffBlockDE);

	return mapLocation;
}


int initRootDirectory() {
	//directoryEntry dEntries[INIT_NUM_OF_DIRECT];
	directoryEntry * dEntries = (directoryEntry *)calloc(INIT_NUM_OF_DIRECT, DE_SIZE);

	int numBlocks = INIT_NUM_OF_DIRECT / ENTRIES_PER_BLOCK;
    int mapLocation = allocateFirstBlocks(numBlocks);

	for (int i = 0; i < INIT_NUM_OF_DIRECT; i++) {
		directoryEntry * entry = &(dEntries[i]);
		// if first or second entry
        if (i == 0 || i == 1) {
			createEntry(
				entry,									// current entry
				(i == 0 ? "." : ".."),					// self or parent name
				true,									// is a direc
				DE_SIZE * INIT_NUM_OF_DIRECT,			// size of root
				(i == 1 ? dEntries[0].date : time(0)),	// date created, or copy self entry
				mapLocation								// location in volume
			);

			for (int j = 0; j < MAX_EXTENTS; j++) {
				extent * e = &(entry->extentLocations[j]);
				e->blockNumber = 0;
				e->count = 0;
			}
        }
		// all other entries in a "known free state"
		//		(date=-1, name="", fileSize=0, isDirectory=false, extents={0,0})
        else
            createEntry(entry, "", false, 0, -1, -1);
    }

	LBAwrite(dEntries, numBlocks, mapLocation);

	return mapLocation;
}

// For debug purposes for now
directoryEntry* readDirectory(int location, int blocks) {
	// directoryEntry
	// 		char name[27];
	// 		bool isDirectory;
	// 		uint32_t fileSize;
	// 		time_t date;
	// 		extent location[3];
	
	directoryEntry* dEntries[INIT_NUM_OF_DIRECT];

	LBAread(dEntries, blocks, location);

	// Debug
	for (int i = 0; i < INIT_NUM_OF_DIRECT; i++) {
		directoryEntry * entry = dEntries[i];
		printf("%d. [%s][%ld][%d][%d][%d][%d]\n", i, entry->name, entry->date, entry->fileSize, entry->isDirectory, entry->extentLocations[0].blockNumber, entry->extentLocations[0].count);
	}
}
