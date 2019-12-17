#ifndef ABS2REL_PATH_H
#define ABS2REL_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief abs2relPath
 * @param absPath Absolute path to convert (usually a symlink target).
 * @param refPath Reference path (usually a symlink path).
 * @return Relative path. Needs to be freed using {@code free}(3).
 */
char *abs2relPath(const char *absPath, const char *refPath);

#ifdef __cplusplus
}
#endif

#endif /* ABS2REL_PATH_H */
