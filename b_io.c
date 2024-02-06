/**************************************************************
 * Class:  CSC-415-03 - Spring 2023
 * Names: Caimin Rybolt-O’Keefe, Nathan Rennacker, Spencer Holsinger, Suzanna Li 
 * Group Name: CN2S
 * Project: Basic File System
 *
 * File: b_io.c
 *
 * Description: Basic File System - Key File I/O Operations
 *
 **************************************************************/

#include "b_io.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>  // for malloc
#include <string.h>  // for memcpy
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "mfs.h"
#include "fsLow.h"
#include "pathparse.h"

#include <stdbool.h>

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

typedef struct b_fcb {
    char* buff;     // holds the open file buffer
    int index;      // holds the current position in the buffer
    int dataInBuffer;     // holds how many valid bytes are in the buffer

    short lbaPos;                 // current block position. allows working with main + extents
    int flagRDWR;               // flag showing
    short filePos;                // total number of bytes read
    short blocksAtMainLoc;        // number of blocks at main location
                                //   (because this is virtually unknown if bytes exist in extents)
    struct fs_stat* fileInfo;   // access to extents and file size
    directoryEntry* parent;      // DE to edit (if write, creat, or trunc a file)
} b_fcb;

b_fcb fcbArray[MAXFCBS];

int startup = 0;  // Indicates that this has not been initialized

// Method to initialize our file system
void b_init() {
    // init fcbArray to all free
    for (int i = 0; i < MAXFCBS; i++) {
        fcbArray[i].buff = NULL;  // indicates a free fcbArray
    }

    startup = 1;
}

// Method to get a free FCB element
b_io_fd b_getFCB() {
    for (int i = 0; i < MAXFCBS; i++) {
        if (fcbArray[i].buff == NULL) {
            return i;  // Not thread safe (But do not worry about it for this assignment)
        }
    }
    return (-1);  // all in use
}

// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open(char* filename, int flags) {
    b_io_fd returnFd;

    if (startup == 0) b_init();  // Initialize our system

    returnFd = b_getFCB();  // get our own file descriptor
                            // check for error - all used FCB's
    
    // If all FDs in use, or flags are improperly set
    if (returnFd < 0 || flags < 0) {
        fprintf(stderr, "ERROR: All file descriptors in use OR flags set improperly.\n");
        return -1;
    }

    b_fcb * fcb = &(fcbArray[returnFd]);

    // if passed a directory but not creating a file, exit because we dont open directories
    if (fs_isDir(filename) == 1) {
        fprintf(stderr, "ERROR: Directories can't be open as files\n");
        return -2;
    }

    // If fail to get fs_stat information AND not creating a file, return error -2
    fcb->fileInfo = malloc(sizeof(struct fs_stat));
    int fsstatReturnVal = fs_stat(filename, fcb->fileInfo);
    // file does not exists. creating       = false
    // file does not exists. not creating   = true
    // file exists. creating                = false
    // file exists. not creating            = false
    if (fsstatReturnVal < 0 && !(flags & O_CREAT)) {
        fprintf(stderr, "ERROR: File does not exist OR not told to create a file\n");
        return -3;
    }
    
    char * lastSlash = strrchr(filename, '/');
    char * filenameSeparated = NULL;
    if (lastSlash == NULL && fs_isDir(filename) < 1) {
        // get CWD
        int dir_buf_length = 4096;
        char * dir_buf = malloc(dir_buf_length + 1);
        char * ptr = fs_getcwd(dir_buf, dir_buf_length);
        if (ptr == NULL) {
            fprintf(stderr, "ERROR: Failed to grab Current Working Directory.\n");
            free(dir_buf);
            return -4;  // error -4 if it failed to grab the CWD
        }

        // get DE of CWD
        fcb->parent = parsePath(ptr);

        if (fcb->parent == NULL) {
            free(dir_buf);
            fprintf(stderr, "ERROR: Failed to parse Current Working Directory as a path.\n");
            return -5;  // error -5 if it failed to parse
        }

        free(dir_buf);  // free the buffer for cwd path because useless now
    }
    else {
        char * parentPath = malloc(strlen(filename));
        *lastSlash = '\0';
        strcpy(parentPath, filename);
        filenameSeparated = lastSlash + 1;

        fcb->parent = parsePath(parentPath);

        if (fcb->parent == NULL) {
            fprintf(stderr, "ERROR: Parent of path does not exist.\n");
            return -4;
        }

        *lastSlash = '/';

        free(parentPath);
    }
    
    /// Check for WriteOnly / ReadWrite flags using bitwise operators
    /// Default to ReadOnly if neither found
    if (flags & O_RDWR)
        fcb->flagRDWR = O_RDWR;
    else if (flags & O_WRONLY)
        fcb->flagRDWR = O_WRONLY;
    else
        fcb->flagRDWR = O_RDONLY;

    /// Check for other flagsfilename
    /// O_TRUNC
    //      if file exists, set length to 0
    //      file must also be able to be written to
    if ((flags & O_TRUNC) && (flags & (O_RDWR | O_WRONLY)) && fsstatReturnVal == 0) {
        fcb->fileInfo->st_size      = 0;
        fcb->fileInfo->st_blocks    = 0;
        fcb->fileInfo->st_location  = -1;

        // temporary block of DE buffer
        directoryEntry * tempBlockBuf = (directoryEntry *)calloc(ENTRIES_PER_BLOCK, DE_SIZE);

        int blockToEditDE = -1;
        int indexInBlock = -1;
        // for main loc + each extent, look for blank DE or empty extent, whichever comes first
        for (int i = -1; i < MAX_EXTENTS && indexInBlock < 0; i++) {
            int loc = 0;

            // if main loc
            if (i == -1)
                loc = fcb->parent->location;
            // else extent
            else
                loc = fcb->parent->extentLocations[i].blockNumber;
            
            if (loc <= 0)
                continue;

            // read the DEs
            for (int j = 0; j < INIT_NUM_OF_DIRECT; j++) {
                // read a block every 8 entries
                if (j % ENTRIES_PER_BLOCK == 0)
				    LBAread(tempBlockBuf, 1, loc + (j / ENTRIES_PER_BLOCK));
                
                // current DE of block
                directoryEntry * currEntry = &(tempBlockBuf[j % ENTRIES_PER_BLOCK]);

                // If duplicate DE name found, exit with -1
                if (strcmp(currEntry->name, filename) == 0) {
                    blockToEditDE = loc + (j / ENTRIES_PER_BLOCK);
                    indexInBlock = j % ENTRIES_PER_BLOCK;
                    break;
                }
            }
        }

        // Update the entry info to 0
        LBAread(tempBlockBuf, 1, blockToEditDE);

        int blocksAtMainLocation = tempBlockBuf[indexInBlock].fileSize / B_CHUNK_SIZE + (tempBlockBuf[indexInBlock].fileSize % B_CHUNK_SIZE);
        // reset all extent values to base
        for (int i = 0; i < MAX_EXTENTS; i++) {
            if (tempBlockBuf[indexInBlock].extentLocations[i].count > 0) {
                blocksAtMainLocation -= tempBlockBuf[indexInBlock].extentLocations[i].count;

                clearBlocks(
                    tempBlockBuf[indexInBlock].extentLocations[i].blockNumber,
                    tempBlockBuf[indexInBlock].extentLocations[i].count
                );

                tempBlockBuf[indexInBlock].extentLocations[i].blockNumber = 0;
                tempBlockBuf[indexInBlock].extentLocations[i].count = 0;
            }
        }

        if (tempBlockBuf[indexInBlock].location > 0 &&
            tempBlockBuf[indexInBlock].fileSize > 0)
        {
            clearBlocks(tempBlockBuf[indexInBlock].location, blocksAtMainLocation);
            tempBlockBuf[indexInBlock].location = -1;
            tempBlockBuf[indexInBlock].fileSize = 0;
        }

        // write updated entry back
        LBAwrite(tempBlockBuf, 1, blockToEditDE);

        free(tempBlockBuf);
    }

    /// O_CREAT
    //      if file doesn't exist, create it
    //      whether file can be read from or written to is determined above
    if (fsstatReturnVal < 0 && (flags & O_CREAT)) {
        // find a blank DE in curr working direc (at main loc OR in extents)
        int blankDE_blockPos        = -1;
        int blankDE_indexInBlock    = -1;
        int emptyExtentIndex        = -1;
        
        // temporary block of DE buffer
        directoryEntry * tempBlockBuf = (directoryEntry *)calloc(ENTRIES_PER_BLOCK, DE_SIZE);

        // for main loc + each extent, look for blank DE or empty extent, whichever comes first
        for (int i = -1; i < MAX_EXTENTS && blankDE_indexInBlock < 0; i++) {
            int loc = 0;

            // if main loc
            if (i == -1) {
                loc = fcb->parent->location;
            }
            // else extent
            else {
                // exit if empty extent found
                if (fcb->parent->extentLocations[i].count <= 0) {
                    emptyExtentIndex = i;
                    break;
                }
                // otherwise we have to read the extent
                loc = fcb->parent->extentLocations[i].blockNumber;
            }

            if (loc <= 0)
                continue;

            // read the DEs
            for (int j = 0; j < INIT_NUM_OF_DIRECT; j++) {
                // read a block every 8 entries
                if (j % ENTRIES_PER_BLOCK == 0)
				    LBAread(tempBlockBuf, 1, loc + (j / ENTRIES_PER_BLOCK));
                
                // current DE of block
                directoryEntry * currEntry = &(tempBlockBuf[j % ENTRIES_PER_BLOCK]);

                // exit if blank DE found
                if (blankDE_indexInBlock < 0 && currEntry->date < 0) {
                    blankDE_blockPos = loc + (j / ENTRIES_PER_BLOCK);
                    blankDE_indexInBlock = j % ENTRIES_PER_BLOCK;
                    break;
                }
            }
        }

        // if no empty DE or free extent found
        if (blankDE_indexInBlock < 0 && emptyExtentIndex < 0) {
            fprintf(stderr, "ERROR: No free Directory Entries.\n");
            free(tempBlockBuf);

            return -6;
        }

        // create a DE for the new file with filename and size 0 at current time at allocated loc
        directoryEntry * entry = malloc(DE_SIZE);
        if (filenameSeparated == NULL)
            createEntry(entry, filename, false, 0, time(0), -1);
        else
            createEntry(entry, filenameSeparated, false, 0, time(0), -1);

        // if empty extent found
        if (emptyExtentIndex > -1) {
            // allocate for empty extent to fill
            int mapLoc_newExtent = allocateFirstBlocks(INIT_NUM_OF_DIRECT / ENTRIES_PER_BLOCK);
            // if failed to allocate to extent
            if (mapLoc_newExtent < 0) {
                fprintf(stderr, "ERROR: Could not allocate for new Extent.\n");
                free(entry);
                free(tempBlockBuf);

                return -7;
            }

            // Fill every DE of the newly allocated extent with free DEs
            for (int i = 0; i < INIT_NUM_OF_DIRECT; i++) {
                // first entry is the DE of the new file
                if (i == 0) {
                    memcpy(&(tempBlockBuf[i % ENTRIES_PER_BLOCK]), entry, DE_SIZE);
                }
                // others are blank DE
                else
                    createEntry(&(tempBlockBuf[i % ENTRIES_PER_BLOCK]), "", false, 0, -1, -1);
                
                // Write to volume every 8 entries
                if (i % ENTRIES_PER_BLOCK == ENTRIES_PER_BLOCK - 1)
                    LBAwrite(tempBlockBuf, 1, mapLoc_newExtent + (i / ENTRIES_PER_BLOCK));
            }

            // update parent's DE size and extent info
            LBAread(tempBlockBuf, 1, fcb->parent->location);

            tempBlockBuf[0].fileSize += INIT_NUM_OF_DIRECT * DE_SIZE;
            tempBlockBuf[0].extentLocations[emptyExtentIndex].blockNumber = mapLoc_newExtent;
            tempBlockBuf[0].extentLocations[emptyExtentIndex].count = INIT_NUM_OF_DIRECT / ENTRIES_PER_BLOCK;

            LBAwrite(tempBlockBuf, 1, fcb->parent->location);
        }
        // otherwise empty DE found
        else {
            LBAread(tempBlockBuf, 1, blankDE_blockPos);
            copyEntry(&tempBlockBuf[blankDE_indexInBlock], entry->name, false, 0, entry->date, -1, (entry->extentLocations));
            LBAwrite(tempBlockBuf, 1, blankDE_blockPos);
        }
        free(tempBlockBuf);

        free(entry);

        // grab fs_stat info again because it should exist now
        if (filenameSeparated != NULL) {
            // if it failed to grab, its because in remote direc
            //*lastSlash = '/';
            //printf("[%s]\n", filename);

            fs_stat(filename, fcb->fileInfo);
            //*lastSlash = '\0';
        }
        else
            fs_stat(filename, fcb->fileInfo);
    }

    // Allocate buffer of CHUNK size, return -3 if error
    fcb->buff = malloc(sizeof(char) * B_CHUNK_SIZE);
	if (fcb->buff == NULL) {
        free(fcb->parent);
        fcb->parent = NULL;
        fprintf(stderr, "ERROR: Could not malloc for the buffer\n");
        return -3;
    }
    
    fcb->index      = 0;
    fcb->dataInBuffer     = 0;
    fcb->filePos    = 0;
    fcb->lbaPos     = fcb->fileInfo->st_location;
    
    fcb->blocksAtMainLoc = fcb->fileInfo->st_blocks;
    for (int i = 0; i < MAX_EXTENTS && fcb->fileInfo->st_size > 0; i++) {
        int count = fcb->fileInfo->st_extents[i].count;

        if (count > 0)
            fcb->blocksAtMainLoc -= count;
    }

    return (returnFd);  // all set
}

// Interface to seek function
int b_seek(b_io_fd fd, off_t offset, int whence) {
    if (startup == 0) b_init();  // Initialize our system

    // check that fd is between 0 and (MAXFCBS-1)
    if ((fd < 0) || (fd >= MAXFCBS)) {
        return (-1);  // invalid file descriptor
    }

    b_fcb * fcb = &(fcbArray[fd]);  // Current FCB

    // If buffer is NULL, Invalid FD, Error
    if (fcb->buff == NULL || fcb->fileInfo == NULL)
        return -1;

    if (fcb->lbaPos < 0)
        return 0;
    
    // Current index in the file
    int fileIndex = fcb->filePos;
    int fileSize = fcb->fileInfo->st_size;
    int seekPos = 0;

    // IF Seek from beginning of file
    // IF Seek from current index in file
    // IF Seek from end of file
    // ELSE Invalid whence, Error
    if (whence == SEEK_SET)         seekPos = 0 + offset;
    else if (whence == SEEK_CUR)    seekPos = fileIndex + offset;
    else if (whence == SEEK_END)    seekPos = fileSize + offset;
    else                            return -2;

    // IF New position below 0
    // IF New position above file size
    if (seekPos < 0)                seekPos = 0;
    else if (seekPos > fileSize)    seekPos = fileSize;
    
    // If position changed
    if (seekPos != fileIndex) {
        // Update file position
        fcb->filePos = seekPos;

        // Update lba position
        int blkOffsetFromBeg = seekPos / B_CHUNK_SIZE;
        for (int i = -1; i < MAX_EXTENTS; i++) {
            // If mainLoc
            if (i == -1)
            {
                // If the difference is >0, range is [mainLoc, mainLoc + blockAtMainLoc)
                if (fcb->blocksAtMainLoc - blkOffsetFromBeg > 0) {
                    fcb->lbaPos = fcb->fileInfo->st_location + blkOffsetFromBeg;
                    break;
                }

                // Else subtract blocks at main because range is elsewhere
                blkOffsetFromBeg -= fcb->blocksAtMainLoc;
            }
            // Otherwise in an extent
            else {
                extent * ext = &(fcb->fileInfo->st_extents[i]);

                // If the difference is >0, range is [blockNumber, blockNumber + count)
                if (ext->count - blkOffsetFromBeg > 0) {
                    fcb->lbaPos = ext->blockNumber + blkOffsetFromBeg;
                    break;
                }

                // Else subtract count because range is in other extent
                blkOffsetFromBeg -= ext->count;
            }
        }

        // Update buffer index
        fcb->index = seekPos % B_CHUNK_SIZE;

        // Update the buffer to the current, as long as not at EOF
        if (seekPos != fileSize) {
            LBAread(fcb->buff, 1, fcb->lbaPos);
            fcb->dataInBuffer = 1;
        }
    }

    return seekPos;
}

// Interface to write function
int b_write(b_io_fd fd, char* buffer, int count) {
    if (startup == 0) b_init();  // Initialize our system

    // check that fd is between 0 and (MAXFCBS-1)
    if ((fd < 0) || (fd >= MAXFCBS)) {
        fprintf(stderr, "ERROR: Invalid file descriptor.\n");
        return (-1);  // invalid file descriptor
    }

    b_fcb * fcb = &(fcbArray[fd]);  // Current FCB

    // Check if FD is valid (if buffer exists)
    if (fcb->buff == NULL || fcb->fileInfo == NULL) {
        fprintf(stderr, "ERROR: Invalid file descriptor.\n");
        return -1;
    }

    // Check if FD opened with neither WriteOnly or ReadWrite
    //      return error -2 if ReadOnly
    if ( !(fcb->flagRDWR & (O_WRONLY | O_RDWR)) || count < 0) {
        fprintf(stderr, "ERROR: File not opened with Write permissions.\n");
        return -2;
    }
    
    if (count == 0)
        return 0;

    int fileLoc = fcb->fileInfo->st_location;
    int fileSize = fcb->fileInfo->st_size;
    int bytesBuffered = 0;

    // If file has no location allocated
    if (fcb->lbaPos <= 0) {
        // allocate some blocks
        int blocksNeeded = (count + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE;
        int afbReturn = allocateFirstBlocks(blocksNeeded);
        // if couldn't allocate blocks
        if (afbReturn < 0) {
            fprintf(stderr, "ERROR: Could not allocate initial blocks for file.\n");
            return -3;
        }
        
        fcb->lbaPos = afbReturn;
        fileLoc = afbReturn;
        fcb->blocksAtMainLoc = blocksNeeded;
        fcb->fileInfo->st_location = afbReturn;
        fcb->fileInfo->st_blocks = blocksNeeded;

        // temporary block of DE buffer
        directoryEntry * tempBlockBuf = (directoryEntry *)calloc(ENTRIES_PER_BLOCK, DE_SIZE);

        int blockToEditDE = -1;
        int indexInBlock = -1;
        // for main loc + each extent, look for blank DE or empty extent, whichever comes first
        for (int i = -1; i < MAX_EXTENTS && indexInBlock < 0; i++) {
            int loc = 0;

            // if main loc
            if (i == -1)
                loc = fcb->parent->location;
            // else extent
            else
                loc = fcb->parent->extentLocations[i].blockNumber;
            
            if (loc <= 0)
                continue;

            // read the DEs
            for (int j = 0; j < INIT_NUM_OF_DIRECT; j++) {
                // read a block every 8 entries
                if (j % ENTRIES_PER_BLOCK == 0)
				    LBAread(tempBlockBuf, 1, loc + (j / ENTRIES_PER_BLOCK));
                
                // current DE of block
                directoryEntry * currEntry = &(tempBlockBuf[j % ENTRIES_PER_BLOCK]);

                // If DE found, break
                if (strcmp(currEntry->name, fcb->fileInfo->st_name) == 0) {
                    blockToEditDE = loc + (j / ENTRIES_PER_BLOCK);
                    indexInBlock = j % ENTRIES_PER_BLOCK;
                    break;
                }
            }
        }

        // update entry in info volume
        LBAread(tempBlockBuf, 1, blockToEditDE);

        tempBlockBuf[indexInBlock].location = afbReturn;
        
        LBAwrite(tempBlockBuf, 1, blockToEditDE);

        free(tempBlockBuf);
    }   
    // If filePos + count > EOF pos, and we have to write more characters, so allocate more blocks
    else if ((fileSize + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE < ((fcb->filePos + count) + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE) {
        // Number of blocks to add based on how many characters are to written past the file size
        int blocksNeeded = (((fcb->filePos + count) + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE) - ((fileSize + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE);

        int aabReturn = allocateAdditionalBlocks(
            fcb->fileInfo->st_location,
            fcb->blocksAtMainLoc,
            blocksNeeded,
            fcb->fileInfo->st_extents
        );

        // If couldn't allocate more blocks, error -3
        if (aabReturn < 0) {
            fprintf(stderr, "ERROR: Could not allocate new blocks for file.\n");
            return -3;
        }
        
        if (aabReturn == 0)
            fcb->blocksAtMainLoc += blocksNeeded;
        
        // Update fileInfo values
        fcb->fileInfo->st_blocks += blocksNeeded;
    }

    // While there are bytes to be written
    while (count != bytesBuffered) {
        // If we can write entire contiguous LBAs, requires useable bytes in buffer to be 0
        int wholeLBAs = (count - bytesBuffered) / B_CHUNK_SIZE;
        if (wholeLBAs > 0 && fcb->dataInBuffer == 0) {
            // Check if lbaPos within range of mainLoc [main location, main location furthest block]
            if (fcb->fileInfo->st_location <= fcb->lbaPos &&
                fcb->lbaPos < fcb->fileInfo->st_location + fcb->blocksAtMainLoc)
            {
                wholeLBAs = (fcb->fileInfo->st_location + fcb->blocksAtMainLoc) - fcb->lbaPos;
            }
            // If lbaPos not in range of mainLoc, check each extent
            else {
                for (int i = 0; i < MAX_EXTENTS; i++) {
                    extent * currExt = &(fcb->fileInfo->st_extents[i]);

                    // Check if valid extent
                    if (currExt->count > 0 &&
                        // Check if lbaPos within range of extent [extent location, extent location furthest block]
                        currExt->blockNumber <= fcb->lbaPos &&
                        fcb->lbaPos < currExt->blockNumber + currExt->count)
                    {
                        wholeLBAs = (currExt->blockNumber + currExt->count) - fcb->lbaPos;
                        break;
                    }
                }
            }

            fcb->lbaPos += LBAwrite(buffer + bytesBuffered, wholeLBAs, fcb->lbaPos);
            bytesBuffered += wholeLBAs * B_CHUNK_SIZE;
        }
        else {
            // Read a block if buffer has no bytes
            if (fcb->dataInBuffer == 0) {
                LBAread(fcb->buff, 1, fcb->lbaPos);
                
                fcb->dataInBuffer = 1;
            }

            // bytes to memcpy is default bytes left to write
			// but if amount left to write is greater than chunk size
			// then change to difference of chunk size and buffer pos
            int bytesToCopy = count - bytesBuffered;
            if (bytesToCopy + fcb->index > B_CHUNK_SIZE)
                bytesToCopy = B_CHUNK_SIZE - fcb->index;
            
            // copy to file_buffer+offset from buffer+bytes_written an amount determined above
            memcpy(fcb->buff + fcb->index, buffer + bytesBuffered, bytesToCopy);
            bytesBuffered   += bytesToCopy;
            fcb->index      += bytesToCopy;
            //fcb->buflen     -= bytesToCopy;

            // Write the block back
            LBAwrite(fcb->buff, 1, fcb->lbaPos);

            // if entire buffer writtern: data in buffer is useless
            if (fcb->index == B_CHUNK_SIZE) {
                fcb->dataInBuffer = 0;    
                fcb->index  = 0;    // reset buffer position to 0
                fcb->lbaPos += 1;
            }
        }

        // check if lbaPos is 1 above useable blocks
        for (int i = -1; i < MAX_EXTENTS; i++) {
            // if 1 above main location, set location to first extent
            if (i == -1 &&
                fcb->lbaPos == fcb->fileInfo->st_location + fcb->blocksAtMainLoc &&
                fcb->fileInfo->st_extents[i + 1].count > 0)
            {
                fcb->lbaPos = fcb->fileInfo->st_extents[i + 1].blockNumber;
                break;
            }
            // except for last extent, if 1 above extent, set location to next extent
            else if (i < MAX_EXTENTS - 1 &&
                fcb->lbaPos == fcb->fileInfo->st_extents[i].blockNumber + fcb->fileInfo->st_extents[i].count &&
                fcb->fileInfo->st_extents[i + 1].count > 0)
            {
                fcb->lbaPos = fcb->fileInfo->st_extents[i + 1].blockNumber;
                break;
            }
        }
    }

    fcb->filePos += bytesBuffered;      // update total number of bytes written
    if (fcb->filePos > fcb->fileInfo->st_size) {
        fcb->fileInfo->st_size = fcb->filePos;

        // temporary block of DE buffer
        directoryEntry * tempBlockBuf = (directoryEntry *)calloc(ENTRIES_PER_BLOCK, DE_SIZE);

        int blockToEditDE = -1;
        int indexInBlock = -1;
        // for main loc + each extent, look for matching entry
        for (int i = -1; i < MAX_EXTENTS && indexInBlock < 0; i++) {
            int loc = 0;

            // if main loc
            if (i == -1)
                loc = fcb->parent->location;
            // else extent
            else
                loc = fcb->parent->extentLocations[i].blockNumber;
            
            if (loc <= 0)
                continue;

            // read the DEs
            for (int j = 0; j < INIT_NUM_OF_DIRECT; j++) {
                // read a block every 8 entries
                if (j % ENTRIES_PER_BLOCK == 0)
				    LBAread(tempBlockBuf, 1, loc + (j / ENTRIES_PER_BLOCK));
                
                // current DE of block
                directoryEntry * currEntry = &(tempBlockBuf[j % ENTRIES_PER_BLOCK]);

                // If DE found, break
                if (strcmp(currEntry->name, fcb->fileInfo->st_name) == 0) {
                    blockToEditDE = loc + (j / ENTRIES_PER_BLOCK);
                    indexInBlock = j % ENTRIES_PER_BLOCK;
                    break;
                }
            }
        }

        // update entry in info volume
        LBAread(tempBlockBuf, 1, blockToEditDE);

        tempBlockBuf[indexInBlock].fileSize = fcb->fileInfo->st_size;
        
        LBAwrite(tempBlockBuf, 1, blockToEditDE);

        free(tempBlockBuf);
    }

    return bytesBuffered;               // return number of bytes that were written
}

// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read(b_io_fd fd, char* buffer, int count) {
    if (startup == 0) b_init();  // Initialize our system

    // check that fd is between 0 and (MAXFCBS-1)
    if ((fd < 0) || (fd >= MAXFCBS)) {
        fprintf(stderr, "ERROR: Invalid file descriptor.\n");
        return (-1);  // invalid file descriptor
    }

    b_fcb * fcb = &(fcbArray[fd]);  // Current FCB
    
    // Check if FD is valid (if buffer exists)
    if (fcb->buff == NULL || fcb->fileInfo == NULL || count < 0) {
        fprintf(stderr, "ERROR: Invalid file descriptor.\n");
        return -1;
    }

    // Check if FD intialized with flag of WriteOnly
    if (fcb->flagRDWR & O_WRONLY) {
        fprintf(stderr, "ERROR: File not opened with Read permissions.\n");
        return -2;
    }
    
    // If file has no size, return 0
    if (fcb->fileInfo->st_size == 0 || fcb->lbaPos < 0)
        return 0;
    
    int fileLoc = fcb->fileInfo->st_location;
    int fileSize = fcb->fileInfo->st_size;
    int bytesBuffered = 0;

    // Clamp the count to be read:     min(count, fileSize - filePos)
    if (fileSize - fcb->filePos < count)
        count = fileSize - fcb->filePos;

    if (count == 0)
        return 0;

    // While there are bytes to be read
    while (count != bytesBuffered) {
        // If we can read entire contiguous LBAs, requires useable bytes in buffer to be 0
        int wholeLBAs = (count - bytesBuffered) / B_CHUNK_SIZE;
        if (wholeLBAs > 0 && fcb->dataInBuffer == 0) {
            // Check if lbaPos within range of mainLoc [main location, main location furthest block]
            if (fcb->fileInfo->st_location <= fcb->lbaPos &&
                fcb->lbaPos < fcb->fileInfo->st_location + fcb->blocksAtMainLoc)
            {
                wholeLBAs = (fcb->fileInfo->st_location + fcb->blocksAtMainLoc) - fcb->lbaPos;
            }
            // If lbaPos not in range of mainLoc, check each extent
            else {
                for (int i = 0; i < MAX_EXTENTS; i++) {
                    extent * currExt = &(fcb->fileInfo->st_extents[i]);

                    // Check if valid extent
                    if (currExt->count > 0 &&
                        // Check if lbaPos within range of extent [extent location, extent location furthest block]
                        currExt->blockNumber <= fcb->lbaPos &&
                        fcb->lbaPos < currExt->blockNumber + currExt->count)
                    {
                        wholeLBAs = (currExt->blockNumber + currExt->count) - fcb->lbaPos;
                        break;
                    }
                }
            }

            fcb->lbaPos += LBAread(buffer + bytesBuffered, wholeLBAs, fcb->lbaPos);
            bytesBuffered += wholeLBAs * B_CHUNK_SIZE;
        }
        else {
            // Read a block if buffer has no bytes
            if (fileSize - (bytesBuffered + fcb->filePos) > 0 && fcb->dataInBuffer == 0) {
                fcb->lbaPos += LBAread(fcb->buff, 1, fcb->lbaPos);
                fcb->dataInBuffer = 1;
            }


            // bytes to memcpy is default bytes left to read
			// but if amount left to read is greater than chunk size
			// then change to difference of chunk size and buffer pos
            int bytesToCopy = count - bytesBuffered;
            if (bytesToCopy + fcb->index > B_CHUNK_SIZE)
                bytesToCopy = B_CHUNK_SIZE - fcb->index;
            
            // copy to arg buffer+offset, from file buffer+offset, an amount determined above
            memcpy(buffer + bytesBuffered, fcb->buff + fcb->index, bytesToCopy);
            bytesBuffered   += bytesToCopy;
            fcb->index      += bytesToCopy;


            // if entire buffer read: data in buffer is useless
            if (fcb->index == B_CHUNK_SIZE) {
                fcb->dataInBuffer = 0;    // no readable bytes left
                fcb->index  = 0;    // reset buffer position to 0
            }
        }

        // check if lbaPos is 1 above useable blocks
        for (int i = -1; i < MAX_EXTENTS; i++) {
            // if 1 above main location, set location to first extent
            if (i == -1 &&
                fcb->lbaPos == fcb->fileInfo->st_location + fcb->blocksAtMainLoc)
            {
                fcb->lbaPos = fcb->fileInfo->st_extents[i + 1].blockNumber;
                break;
            }
            // except for last extent, if 1 above extent, set location to next extent
            else if (i < MAX_EXTENTS - 1 &&
                fcb->lbaPos == fcb->fileInfo->st_extents[i].blockNumber + fcb->fileInfo->st_extents[i].count)
            {
                fcb->lbaPos = fcb->fileInfo->st_extents[i + 1].blockNumber;
                break;
            }
        }
    }

    fcb->filePos += bytesBuffered;      // update total number of bytes read
    return bytesBuffered;               // return number of bytes that were read
}


// Interface to Close the file
int b_close(b_io_fd fd) {
    // check that fd is between 0 and (MAXFCBS-1)
    if ((fd < 0) || (fd >= MAXFCBS)) {
        fprintf(stderr, "ERROR: Invalid file descriptor.\n");
        return (-1);  // invalid file descriptor
    }

    b_fcb * fcb = &(fcbArray[fd]);  // Current FCB

    if (fcb->buff != NULL)
        free(fcb->buff);        // Free buffer
    
    if (fcb->fileInfo != NULL)
        free(fcb->fileInfo);    // Free file stat
    
    if (fcb->parent != NULL)
        free(fcb->parent);

    fcb->buff       = NULL;
    fcb->fileInfo   = NULL;
    fcb->parent     = NULL;
    fcb->lbaPos     = -1;
    fcb->filePos    = 0;
    fcb->index      = 0;
}
