
#include "fs.h"
// APIs for using the filesystem
#include "allocator.h"
#include "strings.h"
#include "types.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <copyfile.h>

// Opens a file from a path, with write permissions
static File file_open(String path, FileOpenFlag flags) {
    STACK_ALLOC_BEGIN(512);
    char *cstr = cstring_from_string(STACK_ALLOC, path);
	int oflag = O_CREAT;
    if (flags & FileOpen_ReadOnly)
		oflag |= O_RDONLY;
    else
        oflag |= O_RDWR;
	if (flags & FileOpen_Append)
		oflag |= O_APPEND;

	// TODO replace fopen with open
    // TODO handle append write

    int fd = open(cstr, oflag);
    if (fd < 0) {
        err("Failed to open %s: %s\n", cstr, strerror(errno));
        return (File){0};
    }
    usize len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    return (File){.path = path, .fd = fd, .size = len};
}

static bool32 file_is_valid(File f) {
	return (f.path.data && f.path.len && f.fd >= 0);
}

static bool32 file_exists(String path) {
    STACK_ALLOC_BEGIN(512);
    char *cstr = cstring_from_string(STACK_ALLOC, path);
    struct stat s;
    return stat(cstr, &s) == 0;
}

static void file_seek_begin(File f) {
    lseek(f.fd, 0, SEEK_SET);
}

static void file_seek_end(File f) {
    lseek(f.fd, 0, SEEK_END);
}

static bool32 file_read_full(File f, char *buf) {
    usize bytes_read = read(f.fd, buf, f.size);
    return bytes_read == f.size;
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
    write(f.fd, data, size);
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
    close(f.fd);
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
    if (!file_exists(src)) {
        err("Cannot copy file: %.*s -- does not exist\n", src.len, src.data);
        return FALSE;
    }

    if (!file_exists(dst)) {
        if (!make_dir(dst)) {
            return FALSE;
        }
    }
    Dir src_dir = dir_open(src);
    DirIterator iter = dir_iter_next(src_dir);
    for (; iter.ok; iter = dir_iter_next(src_dir)) {
        if (string_match(iter.path, STR_LIT(".")) || string_match(iter.path, STR_LIT(".."))) {
            continue;
        }
        STACK_ALLOC_BEGIN(512);
        if (iter.type == FileType_Dir) {
            String path = iter.path; // this is relative to `src` directory
            String src_path = path_join(STACK_ALLOC, (String[]){src, path}, 2);
            String dest_path = path_join(STACK_ALLOC, (String[]){dst, path}, 2);
            // recursively copy this dir
            file_copy_recursive(src_path, dest_path);
        } else {
            String path = iter.path;
            String src_path = path_join(STACK_ALLOC, (String[]){src, path}, 2);
            String dest_path = path_join(STACK_ALLOC, (String[]){dst, path}, 2);
            if (!file_copy(src_path, dest_path)) {
                return FALSE;
            }
        }
    }
    dir_close(src_dir);

    return TRUE;
}

static bool32 file_rename(String new_name, String old_name) {
    bool32 result = TRUE;
    STACK_ALLOC_BEGIN(512);
    const char *cnew = cstring_from_string(STACK_ALLOC, new_name);
    const char *cold = cstring_from_string(STACK_ALLOC, old_name);
    if (rename(cold, cnew) != 0) {
        err("Failed to rename %s to %s: %s\n", cold, cnew, strerror(errno));
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

static usize file_mtime(String path) {
    struct stat s;
    STACK_ALLOC_BEGIN(512);
    const char *cstr = cstring_from_string(STACK_ALLOC, path);
    if (stat(cstr, &s) != 0) {
        err("%s\n", strerror(errno));
        return 0;
    }
    return s.st_mtime;
}

static String file_ext(String path) {
    return string_split_after(path, '.');
}

static bool32 _make_dir_internal(String path) {
    STACK_ALLOC_BEGIN(512);
    char *cstr = cstring_from_string(STACK_ALLOC, path);
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
    const char *ptr = path.data;
	int i;
    for (i = 0; i < path.len; ++i) {
        if (ptr[i] == '/' && i != 0) {
            String dir = {.data = ptr, .len = i};
            if (!_make_dir_internal(dir))
                return FALSE;
        }
    }

    if (!_make_dir_internal(path)) {
        return FALSE;
    }

    return TRUE;
}

static bool32 make_dir(String path) {
    if (file_exists(path)) {
        return FALSE;
    }
    return _make_dir_recursive(path);
}

static Dir dir_open(String path) {
    STACK_ALLOC_BEGIN(512);
    const char *cstr = cstring_from_string(STACK_ALLOC, path);
    DIR *handle = opendir(cstr);
    if (!handle) {
        err("Failed to open %.*s -- %s", path.len, path.data, strerror(errno));
        return (Dir){0};
    }
    Dir dir = {.path = path, .handle = handle};

    return dir;
}

static void dir_close(Dir dir) {
    if (closedir(dir.handle) < 0) {
        err("Failed to close dir %.*s -- %s", dir.path.len, dir.path.data, strerror(errno));
    }
}

static DirIterator dir_iter_next(Dir dir) {
    DirIterator iter = {0};
    iter.file_info = readdir(dir.handle);
    if (!iter.file_info) {
        return iter;
    }
    iter.path = (String){.data = iter.file_info->d_name, .len = iter.file_info->d_namlen};
    if (iter.file_info->d_type & DT_DIR) {
        iter.type = FileType_Dir;
    } else if (iter.file_info->d_type & DT_REG) {
        iter.type = FileType_File;
    }

    iter.ok = TRUE;

    return iter;
}
