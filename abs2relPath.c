#include "abs2relPath.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char *abs2relPath(const char *const absPath, const char *const refPath)
{
    if (absPath[0] != '/' // path to convert is not absolute
            || refPath[0] != '/' // reference path not useable
            )
    {
        return strdup(absPath);
    }
    const size_t refPathLen = strlen(refPath);
    size_t pathElemCount = 0; // Different path elements
    size_t diffPathIndex = 0;
    char diffFound = 0;
    size_t i; /* Declaration in for-loop requires c99 which causes other issues */
    for (i = 0; i < refPathLen; ++i)
    {
        if (refPath[i] == '/')
        {
            if (diffFound)
            {
                ++pathElemCount;
            }
            else
            {
                diffPathIndex = i;
            }
        }
        if (!diffFound // Don't continue after diff, absPath may be shorter!
                && absPath[i] != refPath[i])
        {
            diffFound = 1;
        }
    }
    if (pathElemCount == 0)
    {
        return strdup(absPath); // Paths are equal
    }
    const size_t absPathLen = strlen(absPath);
    char *relPath = malloc(pathElemCount * 3 // "../" for each path element
                           + absPathLen - diffPathIndex // Part to be copied
                           + 1); // Trailing 0
    size_t relPathIndex = 0;
    for (i = 0; i < pathElemCount; ++i)
    {
        relPath[relPathIndex++] = '.';
        relPath[relPathIndex++] = '.';
        relPath[relPathIndex++] = '/';
    }
    memcpy(relPath + relPathIndex, absPath + diffPathIndex + 1, // Start at index + 1 to avoid "//"
           absPathLen + 1 // Original path len
           - (diffPathIndex + 1)); // -(+1) = -1 for skipped '/'
    return relPath;
}
