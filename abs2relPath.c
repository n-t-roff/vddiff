#include "abs2relPath.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"

char *abs2relPath(const char *const absPath, const char *const refPath)
{
#   if defined(TRACE) && 1
    fprintf(debug, "->abs2relPath(absPath=\"%s\", refPath=\"%s\")\n", absPath, refPath);
#   endif
    if (absPath[0] != '/' // path to convert is not absolute
            || refPath[0] != '/' // reference path not useable
            )
    {
#       if defined(TRACE) && 1
        fprintf(debug, "<-abs2relPath: invalid arguments\n");
#       endif
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
#   if defined(TRACE) && 1
    fprintf(debug, "<-abs2relPath: \"%s\"\n", relPath);
#   endif
    return relPath;
}
