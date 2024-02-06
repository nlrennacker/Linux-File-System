/**************************************************************
 * Class:  CSC-415
 * Name: 
 * Student ID: N/A
 * Project: Basic File System
 *
 * File: mfs.h
 *
 * Description:
 *	This is the file system interface.
 *	This is the interface needed by the driver to interact with the file system
 *
 **************************************************************/
#ifndef _MFS_H
#define _MFS_H
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "b_io.h"
#include "directoryEntry.h"
#define FT_REGFILE DT_REG
#define FT_DIRECTORY DT_DIR
#define FT_LINK DT_LNK

#ifndef uint64_t
typedef u_int64_t uint64_t;
#endif
#ifndef uint32_t
typedef u_int32_t uint32_t;
#endif

extern uint32_t CURRENT_DIR_LOC;

// This structure is returned by fs_readdir to provide the caller with information
// about each file as it iterates through a directory
struct fs_diriteminfo {
    unsigned short d_reclen; /* length of this record */
    unsigned char fileType;
    char d_name[256]; /* filename max filename is 255 characters */
};

typedef struct {
    unsigned short d_reclen;         /*length of this record*/
    unsigned short dirEntryPosition; /*directory entry position eg offset from the start of the block */
    uint64_t directoryStartLocation; /*Starting LBA of directory */
} fdDir;
extern fdDir curWorkingDir;
// Key directory functions

/**
 * Creates a new directory at the given path with the specified mode.
 *
 * @param pathname The path where the new directory should be created.
 * @param mode The mode (permissions) for the new directory.
 * @return 0 on success, -1 on failure.
 */
int fs_mkdir(const char *pathname, mode_t mode);


int fs_rmdir(const char *pathname);

/**
 * Opens a directory for reading.
 *
 * @param pathname The path of the directory to open.
 * @return A pointer to the opened directory structure (fdDir) on success, NULL on failure.
 */
fdDir *fs_opendir(const char *pathname);

/**
 * Reads the next directory entry from the given directory structure.
 *
 * @param dirp A pointer to the opened directory structure.
 * @return A pointer to the directory entry information (fs_diriteminfo) on success, NULL on failure.
 */
struct fs_diriteminfo *fs_readdir(fdDir *dirp);

/**
 * Closes the given directory structure.
 *
 * @param dirp A pointer to the opened directory structure.
 * @return 0 on success.
 */
int fs_closedir(fdDir *dirp);


/**
 * Gets the current working directory.
 *
 * @param pathname A buffer to store the current working directory path.
 * @param size The size of the buffer.
 * @return A pointer to the pathname buffer on success, NULL on failure.
 */
char *fs_getcwd(char *pathname, size_t size);

/**
 * Sets the current working directory.
 *
 * @param pathname The path of the new working directory.
 * @return 0 on success, -1 if the path is not found, -2 if the path is not a directory.
 */
int fs_setcwd(char *pathname);

/**
 * Checks if the given path is a file.
 *
 * @param filename The path to check.
 * @return 1 if the path is a file, 0 if it's a directory, -1 if the path is not found.
 */
int fs_isFile(char *filename);

/**
 * Checks if the given path is a directory.
 *
 * @param pathname The path to check.
 * @return 1 if the path is a directory, 0 if it's a file, -1 if the path is not found.
 */
int fs_isDir(char *pathname);

/**
 * Deletes the specified file.
 *
 * @param filename The name of the file to delete.
 * @return 0 on success, -1 on failure.
 */
int fs_delete(char *filename);  // removes a file

/**
 * Moves a file or directory from a source path to a destination path.
 * 
 * @param srcPathname The source path of the file or directory to be moved.
 * @param destPathname The destination path where the file or directory should be moved to.
 * 
 * @return 0 if the operation is successful.
 *         -1 if the source file or directory does not exist.
 *         -2 if the destination folder does not exist.
 *         -3 if the destination is a file, not a directory.
 *         -4 if a file or directory with the same name already exists at the destination.
 */
int fs_move(const char* srcPathname, const char* destPathname);

// This is the structure that is filled in from a call to fs_stat
struct fs_stat {
    off_t st_size;        /* total size, in bytes */
    blksize_t st_blksize; /* blocksize for file system I/O */
    blkcnt_t st_blocks;   /* number of 512B blocks allocated */
    time_t st_accesstime; /* time of last access */
    time_t st_modtime;    /* time of last modification */
    time_t st_createtime; /* time of last status change */

    /* add additional attributes here for your file system */
    short st_location;
    extent st_extents[MAX_EXTENTS];
    char st_name[PATH_MAX];
};

/**
 * Retrieves file or directory information for the given path.
 *
 * @param path The path of the file or directory.
 * @param buf A pointer to the fs_stat structure to store the information.
 * @return 0 on success, -1 on failure.
 */
int fs_stat(const char *path, struct fs_stat *buf);

#endif
