#ifndef FS_H
#define FS_H

// APIs for using the filesystem
#include "allocator.h"
#include "types.h"
#include "strings.h"

#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <copyfile.h>

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

// TODO Change const char * to String where needed, temp allocate a cstring within each function

// Opens a file from a path, with write permissions
static File file_open(String path, FileOpenFlag flags) {
    STACK_ALLOC_BEGIN(512);
    char *cstr = cstring_from_string(STACK_ALLOC, path);
    char *mode;
    if (flags & FileOpen_ReadOnly)
        mode = "r";
    else
        mode = "w";

    // TODO handle append write

    FILE *fd = fopen(cstr, mode);
    if (!fd) {
        err("Failed to open %s: %s\n", cstr, strerror(errno));
        return (File){0};
    }
    usize len = 0;
    fseek(fd, 0, SEEK_END);
    len = (usize)ftell(fd);
    fseek(fd, 0, SEEK_SET);

    return (File){.fd = fd, .size = len};
}

static bool32 file_exists(String path) {
    STACK_ALLOC_BEGIN(512);
    char *cstr = cstring_from_string(STACK_ALLOC, path);
    struct stat s;
    return stat(cstr, &s) == 0;
}

static void file_seek_begin(File f) {
    fseek(f.fd, 0, SEEK_SET);
}

static void file_seek_end(File f) {
    fseek(f.fd, 0, SEEK_END);
}

static bool32 file_read_full(File f, char *buf) {
    usize read = fread(buf, 1, f.size, f.fd);
    return read == f.size;
}

static char *file_read_full_alloc(File f, Allocator *alloc) {
    char *buf = (char*)alloc->alloc(alloc, f.size);
    if (!file_read_full(f, buf)) {
        alloc->free(alloc, buf);
        return NULL;
    }
    return buf;
}

static void file_write(File f, void *data, usize size) {
    fwrite(data, 1, size, f.fd);
}

static void file_write_string(File f, String str) {
    file_write(f, (u8*)str.data, str.len);
}

static void file_printf(File f, Allocator *alloc, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String printed = string_printfv(alloc, fmt, args);
    va_end(args);

    file_write_string(f, printed);
}

static void file_close(File f) {
    fclose(f.fd);
}

static bool32 file_copy(String src, String dst) {
    STACK_ALLOC_BEGIN(512);
    const char *csrc = cstring_from_string(STACK_ALLOC, src);
    const char *cdst = cstring_from_string(STACK_ALLOC, dst);

    if (!file_exists(src)) {
        err("Cannot copy file: %s -- does not exist\n", csrc);
        return FALSE;
    }

    int res = copyfile(csrc, cdst, NULL, COPYFILE_ALL);
    if (res < 0) {
        err("Failed to copy file: %s -- %s\n", csrc, strerror(errno));
        return FALSE;
    }
    return TRUE;
}

static bool32 file_copy_recursive(String src, String dst) {
    STACK_ALLOC_BEGIN(512);
    const char *csrc = cstring_from_string(STACK_ALLOC, src);
    const char *cdst = cstring_from_string(STACK_ALLOC, dst);

    if (!file_exists(src)) {
        err("Cannot copy file: %s -- does not exist\n", csrc);
        return FALSE;
    }

    int res = copyfile(csrc, cdst, NULL, COPYFILE_ALL | COPYFILE_RECURSIVE);
    if (res < 0) {
        err("Failed to copy file: %s -- %s\n", csrc, strerror(errno));
        return FALSE;
    }
    return TRUE;
}

static bool32 file_rename(const char *name, const char *old) {
    bool32 result = TRUE;
    if (rename(old, name) != 0) {
        err("Failed to rename %s to %s: %s\n", old, name, strerror(errno));
        result = FALSE;
    }

    return result;
}

static bool32 file_delete(String path) {
    STACK_ALLOC_BEGIN(512);
    char *cstr = cstring_from_string(STACK_ALLOC, path);
    if (remove(cstr) != 0) {
        err("Failed to remove file: %s -- %s\n", cstr, strerror(errno));
        return FALSE;
    }
    return TRUE;
}

static usize file_mtime(const char *path) {
    struct stat s;
    if (stat(path, &s) != 0) {
        err("%s\n", strerror(errno));
        return 0;
    }
    return s.st_mtime;
}

typedef struct DirIterator {
    struct dirent *file_info;
    FileType type;
    bool32 ok;
} DirIterator;

static String file_ext(char *path) {
    return string_split_after(string(path), '.');
}

static bool32 dir_exists(String path) {
    STACK_ALLOC_BEGIN(512);
    char *cstr = (char*)STACK_ALLOC->alloc(STACK_ALLOC, path.len + 1);
    memcpy(cstr, path.data, path.len);
    struct stat s;
    int result = stat(cstr, &s);
    return result == 0;
}

static bool32 _make_dir_internal(String path) {
    STACK_ALLOC_BEGIN(512);
    char *cstr = (char*)STACK_ALLOC->alloc(STACK_ALLOC, path.len + 1);
    memcpy(cstr, path.data, path.len);
    int error = mkdir(cstr, 0775);
    if (error != 0) {
        if (errno == EEXIST) {
            println("Dir exists: %s", cstr);
            return TRUE;
        }
        err("Failed to make dir: %s: %d %s\n", cstr, errno, strerror(errno));
        return FALSE;
    }

    dbg("Created dir: %s", cstr);
    return TRUE;
}

static bool32 _make_dir_recursive(String path) {
    char *ptr = path.data;
    for (int i = 0; i < path.len; ++i) {
        if (ptr[i] == '/' && i != 0) {
            String dir = {.data = ptr, .len = i};
            if (!_make_dir_internal(dir))
                return FALSE;
        }
    }

    return TRUE;
}

static bool32 make_dir(String path) {
    if (dir_exists(path)) {
        return FALSE;
    }
    return _make_dir_recursive(path);
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

    iter.ok = TRUE;

    return iter;
}

#endif
