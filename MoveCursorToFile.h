#ifndef MOVE_CURSOR_TO_FILE_H
#define MOVE_CURSOR_TO_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

struct MoveCursorToFileData;

struct MoveCursorToFile {
    void (*destroy)(struct MoveCursorToFile *);
    char *(*getPathName)(struct MoveCursorToFile *);
    char *(*getFileName)(struct MoveCursorToFile *);
    struct MoveCursorToFileData *data;
};

struct MoveCursorToFile *newMoveCursorToFile(const char *const pathName);

#ifdef __cplusplus
}
#endif

#endif /* MOVE_CURSOR_TO_FILE_H */
