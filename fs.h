#ifndef FS_H
#define FS_H

#include "strings.h"
#include "allocator.h"
#include <dirent.h>

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
    String path;
    FILE* fd;
    usize size;
} File;

typedef struct {
    String path;
    DIR *handle;
} Dir;

typedef struct DirIterator {
    struct dirent *file_info;
    String path;
    FileType type;
    bool32 ok;
} DirIterator;

static File file_open(String path, FileOpenFlag flags);
static void file_close(File f);
static bool32 file_exists(String path);
static void file_seek_begin(File f);
static void file_seek_end(File f);
static bool32 file_read_full(File f, char *buf);
static char *file_read_full_alloc(File f, Allocator *alloc);
static void file_write(File f, void *data, usize size);
static void file_write_string(File f, String str);
static void file_printf(File f, Allocator *alloc, const char *fmt, ...);
static bool32 file_copy(String src, String dst);
static bool32 file_copy_recursive(String src, String dst);
static bool32 file_rename(String new_name, String old_name);
static bool32 file_delete(String path);
static usize file_mtime(String path);
static String file_ext(String path);

static bool32 make_dir(String path);
static Dir dir_open(String path);
static void dir_close(Dir dir);
static DirIterator dir_iter_next(Dir dir);

#endif
