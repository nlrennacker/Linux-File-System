/**************************************************************
 * Class:  CSC-415-03 Fall 2023
 * Names: Nathan Rennacker
 * Group Name: CN2S
 * Project: Basic File System
 *
 * File:
 *
 * Description: exposed parsePath function
 * 
 * 
 *
 **************************************************************/
#ifndef _PARSE_PATH_H
#define _PARSE_PATH_H

#include "directoryEntry.h"

/**
 * Parses a given pathname and returns the corresponding directory entry.
 * 
 * This function takes a pathname as input and returns a pointer to a directoryEntry
 * structure corresponding to the last component of the pathname. If the pathname
 * refers to a non-existent entry or an error occurs during parsing, it returns NULL.
 * 
 * @param pathname The input pathname string to be parsed.
 * @return A pointer to a directoryEntry structure if the parsing is successful, NULL otherwise.
 */
directoryEntry *parsePath(const char *pathname);

#endif