#ifndef FS_H
#define FS_H

// APIs for using the filesystem

#include "log.h"
#include "strings.h"

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

typedef u32 FileType;
enum FileTypes {
    FileType_Invalid,
    FileType_File,
    FileType_Dir,
};

typedef struct DirIterator {
    struct dirent *file_info;
    FileType type;
    bool ok;
} DirIterator;

static String file_ext(char *path) {
    return string_split_after(string(path), '.');
}

static DirIterator dir_iter_next(DIR *dir) {
    DirIterator iter = {0};
    iter.file_info = readdir(dir);
    if (!iter.file_info) {
        return iter;
    }
    if (iter.file_info->d_type & DT_DIR) {
        iter.type = FileType_Dir;
    } else if (iter.file_info->d_type & DT_REG) {
        iter.type = FileType_File;
    }

    iter.ok = true;

    return iter;
}

#endif
