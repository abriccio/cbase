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

typedef u32 FileOpenFlag;
enum FileOpenFlags {
    FileOpen_ReadOnly = 1 << 0,
    FileOpen_Binary = 1 << 1,
};

typedef struct {
    FILE* fd;
    usize size;
} File;

// Opens a file from a path, with write permissions
static File file_open(const char *path, FileOpenFlag flags) {
    char *mode;
    if (flags & FileOpen_ReadOnly)
        mode = "r";
    else 
        mode = "w+";
    
    // TODO handle append write

    FILE *fd = fopen(path, mode);
    usize len = 0;
    fseek(fd, 0, SEEK_END);
    len = (usize)ftell(fd);
    fseek(fd, 0, SEEK_SET);
    
    return (File){.fd = fd, .size = len};
}

static void file_seek_begin(File f) {
    fseek(f.fd, 0, SEEK_SET);
}

static void file_seek_end(File f) {
    fseek(f.fd, 0, SEEK_END);
}

static bool file_read_full(File f, u8 *buf) {
    usize read = fread(buf, 1, f.size, f.fd);
    return read == f.size;
}

static u8 *file_read_full_alloc(File f, Allocator *alloc) {
    u8 *buf = (u8*)alloc->alloc(alloc, f.size);
    if (!file_read_full(f, buf)) {
        alloc->free(alloc, buf);
        return NULL;
    }
    return buf;
}

static void file_close(File f) {
    fclose(f.fd);
}

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
