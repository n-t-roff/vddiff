#include "MoveCursorToFile.h"
#include <stdlib.h> /* malloc */
#include <libgen.h> /* dirname, basename */
#include <string.h> /* strdup */

struct MoveCursorToFileData {
    char *dirnameDup;
    char *basenameDup;
    char *pathName;
    char *fileName;
};

static void destroy(struct MoveCursorToFile *this);
static char *getPathName(struct MoveCursorToFile *this);
static char *getFileName(struct MoveCursorToFile *this);

struct MoveCursorToFile *newMoveCursorToFile(const char *const pathName)
{
    struct MoveCursorToFile *this = malloc(sizeof (struct MoveCursorToFile));
    if (this)
    {
        this->destroy = destroy;
        this->getPathName = getPathName;
        this->getFileName = getFileName;
        struct MoveCursorToFileData *data = malloc(sizeof (struct MoveCursorToFileData));
        if (!data)
        {
            destroy(this);
            return NULL;
        }
        data->dirnameDup = strdup(pathName);
        data->basenameDup = strdup(pathName);
        if (!data->dirnameDup || !data->basenameDup)
        {
            destroy(this);
            return NULL;
        }
        data->pathName = dirname(data->dirnameDup);
        data->fileName = basename(data->basenameDup);
        this->data = data;
    }
    return this;
}

static void destroy(struct MoveCursorToFile *this)
{
    if (this->data)
    {
        free(this->data->dirnameDup);
        free(this->data->basenameDup);
        free(this->data);
    }
    free(this);
}

static char *getPathName(struct MoveCursorToFile *this)
{
    return this->data->pathName;
}

static char *getFileName(struct MoveCursorToFile *this)
{
    return this->data->fileName;
}
