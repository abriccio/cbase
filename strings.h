#ifndef STRINGS_H
#define STRINGS_H

#include "allocator.h"
#include "log.h"
#include <wchar.h>

typedef int Utf16BOM;
typedef enum {
    Utf16None = -1,
    Utf16Le,
    Utf16Be,
} Utf16BOMKind;

/* STRING API */

typedef struct {
    char *data;
    int len;
} String;

typedef struct {
    uint16_t *data;
    int len;
} String16;

typedef struct {
    String *strings;
    int count;
    int cap;
} StringArray;

// Find the length of a null-terminated C-string
static int string_len(const char *cstr) {
    int len = 0;
    while(*(cstr++)) {
        len++;
    }
    return len;
}

// For now, retains null-termination
static String string(char *cstr) {
    int len = string_len(cstr);
    return (String){
        .data = cstr,
        .len = len,
    };
}

// returns endianness of string, or -1 for no given byte order
static Utf16BOM string16_bom(const uint16_t first_char) {
    if (first_char == 0xfffe)
        return Utf16Le;
    if (first_char == 0xfeff)
        return Utf16Be;

    return Utf16None;
}

static String string_from_utf16(Allocator *alloc, Utf16BOM byte_order, uint8_t *utf16, size_t utf16_size) {
    String str = {.len = 0};
    char *ptr = str.data = alloc->alloc(alloc, utf16_size / 2 + 1);

    switch (byte_order) {
    case Utf16None:
    case Utf16Le:
        for (int i = 0; i < utf16_size; i += 2, ptr++, str.len++) {
            uint16_t c = *(uint16_t*)&utf16[i];
            *ptr = (char)c;
        }
        *ptr = 0;
        break;
    case Utf16Be:
        for (int i = 0; i < utf16_size; i += 2, ptr++, str.len++) {
            *ptr = utf16[i]; // just ignore lsb lol
        }
        *ptr = 0;
        break;
    }

    return str;
}

static StringArray string_array_from_cstrs(Allocator *alloc, char *cstrs[], int count) {
    StringArray sa = {0};
    String *buf = alloc->alloc(alloc, sizeof(String) * count);
    if (!buf) {
        err("String alloc failed\n");
        return sa;
    }
    sa.strings = buf;
    sa.count = count;
    sa.cap = count;
    for (int i = 0; i < count; ++i) {
        sa.strings[i] = string(cstrs[i]);
    }

    return sa;
}

// Allocates a string array from an array of strings
static StringArray string_array_from_array(Allocator *alloc, String strings[], int count) {
    StringArray sa = {0};
    sa.strings = alloc->alloc(alloc, sizeof(String) * count);
    if (!sa.strings) {
        err("String alloc failed\n");
        return sa;
    }
    for (int i = 0; i < count; ++i) {
        sa.strings[i] = strings[i];
    }

    sa.count = count;
    sa.cap = count;

    return sa;
}

static void string_array_append(Allocator *alloc, StringArray *sa, String str) {
    if (sa->count + 1 >= sa->cap) {
        sa->cap *= 2;
        sa->strings = alloc->realloc(alloc, sa->strings, sa->cap * sizeof(String));
    }

    sa->strings[sa->count++] = str;
}

static String string_concat(Allocator *alloc, String *strings, int count) {
    int size = 0;
    for (int i = 0; i < count; ++i) {
        size += strings[i].len;
    }

    char *out_data = alloc->alloc(alloc, size + 1);
    if (!out_data) {
        err("Allocation failed\n");
        return (String){0};
    }
    String out = (String){
        .data = out_data,
        .len = size,
    };

    for (int i = 0; i < count; ++i) {
        String *s = &strings[i];
        memcpy(out_data, s->data, s->len);
        out_data += s->len;
    }

    *out_data = 0;

    return out;
}

static String path_join(Allocator *alloc, String *paths, int count) {
    int sep_count = count - 1;
    int size = 0;
    for (int i = 0; i < count; ++i) {
        size += paths[i].len;
    }
    String str = {0};
    str.len = sep_count + size;
    char *ptr = str.data = alloc->alloc(alloc, str.len + 1);
    if (!str.data) {
        err("Allocation failed\n");
        return str;
    }
    for (int i = 0; i < count; ++i) {
        String *path = &paths[i];
        memcpy(ptr, path->data, path->len);
        ptr += path->len;
        if (i + 1 != count) {
            memcpy(ptr, "/", 1);
            ptr++;
        }
    }

    *ptr = 0;

    return str;
}

#endif
