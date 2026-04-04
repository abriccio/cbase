#ifndef STRINGS_H
#define STRINGS_H

#include "allocator.h"
#include "log.h"
#include "types.h"
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#include <stdio.h>
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
    const char *data;
    int len;
} ConstString;

typedef struct {
    u16 *data;
    int len;
} String16;

typedef struct {
    String *strings;
    int count;
    int cap;
} StringArray;

static void string_print(String str);
static void string_println(String str);

// Find the length of a null-terminated C-string
static int string_len(const char *cstr) {
    int len = 0;
    while(*cstr) {
        len++;
        cstr++;
    }
    return len;
}

#define STR_LIT(str) (String){.data = (str), .len = sizeof((str))-1}

static String string(char *cstr) {
    int len = string_len(cstr);
    return (String){
        .data = cstr,
        .len = len,
    };
}

static ConstString const_string(const char *cstr) {
    int len = string_len(cstr);
    return (ConstString){
        .data = cstr,
        .len = len,
    };
}

static String string_from_bytes(u8 *bytes, int len) {
    return (String){
        .data = (char*)bytes,
        .len = len,
    };
}

// returns endianness of string, or -1 for no given byte order
static Utf16BOM string16_bom(const u16 first_char) {
    if (first_char == 0xfffe)
        return Utf16Le;
    if (first_char == 0xfeff)
        return Utf16Be;

    return Utf16None;
}

static String string_from_utf16(Allocator *alloc, Utf16BOM byte_order, u8 *utf16, size_t utf16_size) {
    String str = {.len = 0};
    char *ptr = str.data = (char*)alloc->alloc(alloc, utf16_size / 2 + 1);

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

static String16 string16_from_string(Allocator *alloc, String str) {
    String16 str16 = {.len = str.len};

    u16 *ptr = str16.data = (u16*)alloc->alloc(alloc, str.len * sizeof(u16));

    for (int i = 0; i < str.len; ++i) {
        ptr[i] = (u16)str.data[i];
    }

    return str16;
}

static String16 string16_from_utf8(Allocator *alloc, const char *str) {
    int len = string_len(str);
    String16 str16 = {.len = len};

    u16 *ptr = str16.data = (u16*)alloc->alloc(alloc, len * sizeof(u16));

    for (int i = 0; i < len; ++i) {
        ptr[i] = (u16)str[i];
    }

    return str16;
}

static u16 *utf16_from_string(Allocator *alloc, String str) {
    u16 *buf = (u16*)alloc->alloc(alloc, str.len * sizeof(u16) + 1);

    for (int i = 0; i < str.len; ++i) {
        buf[i] = (u16)str.data[i];
    }

    buf[str.len] = 0;

    return buf;
}

static u16 *utf16_from_utf8(Allocator *alloc, const char *str) {
    int len = string_len(str);
    u16 *buf = (u16*)alloc->alloc(alloc, (len + 1) * sizeof(u16));

    for (int i = 0; i < len; ++i) {
        buf[i] = (u16)str[i];
    }

    buf[len] = 0;

    return buf;
}

static bool string_match(String a, String b) {
    if (a.len != b.len) return false;
    if (a.data == b.data) return true;
    for (int i = 0; i < a.len; ++i) {
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }

    return true;
}

static bool const_string_match(ConstString a, ConstString b) {
    if (a.len != b.len) return false;
    if (a.data == b.data) return true;
    for (int i = 0; i < a.len; ++i) {
        if (a.data[i] != b.data[i]) return false;
    }

    return true;
}

// Reminder that uppercase alpha = 65 - 90
// Lowercase alpha = 97 - 122
static bool string_match_no_case(String a, String b) {
    if (a.len != b.len) return false;
    int diff = 0;
    // 'a'-'A'=32, 'z'-'Z'=32
    int case_diff = 32;
    for (int i = 0; i < a.len; ++i) {
        int d = a.data[i] - b.data[i];
        if (d != 0) {
            if (abs(d) != 32) {
                diff += d;
            }
        }
    }

    if (diff == 0)
        return true;
    else
        return false;
}

static StringArray string_array_from_cstrs(Allocator *alloc, char *cstrs[], int count) {
    StringArray sa = {0};
    String *buf = (String*)alloc->alloc(alloc, sizeof(String) * count);
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
    sa.strings = (String*)alloc->alloc(alloc, sizeof(String) * count);
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
    if (sa->count + 1 > sa->cap) {
        sa->cap *= 2;
        sa->strings = (String*)alloc->realloc(alloc, sa->strings, sa->cap * sizeof(String));
    }

    sa->strings[sa->count++] = str;
}

static String string_concat(Allocator *alloc, String *strings, int count) {
    int size = 0;
    for (int i = 0; i < count; ++i) {
        size += strings[i].len;
    }

    char *out_data = (char*)alloc->alloc(alloc, size + 1);
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
    char *ptr = str.data = (char*)alloc->alloc(alloc, str.len + 1);
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

static int string_get_count_of(String str, char c) {
    int count = 0;
    for (int i = 0; i < str.len; ++i) {
        if (str.data[i] == c)
            count += 1;
    }

    return count;
}

static String string_split_until(String str, char delim) {
    char *ptr = str.data;
    char *end = str.data + str.len;
    for (;ptr < end; ptr++) {
        char c = *ptr;
        if (c == delim) {
            return (String){
                .data = str.data,
                .len = (int)(ptr - str.data),
            };
        }
    }
    return (String){0};
}

static String string_split_after(String str, char delim) {
    char *ptr = str.data;
    char *end = str.data + str.len;
    for (;ptr < end; ptr++) {
        char c = *ptr;
        if (c == delim) {
            return (String){
                .data = ptr,
                .len = (int)(end - ptr),
            };
        }
    }
    return (String){0};
}

static StringArray string_split_delim(Allocator *alloc, String str, char delim) {
    StringArray arr = {0};
    int delim_count = string_get_count_of(str, delim);
    arr.cap = delim_count + 1;
    arr.strings = (String*)alloc->alloc(alloc, arr.cap * sizeof(String));
    if (!arr.strings) {
        err("Out of memory\n");
        return arr;
    }
    char *ptr = str.data;
    char *end = str.data + str.len;
    for (;ptr < end;) {
        char *first = ptr;
        for (;ptr < end; ptr++) {
            char c = *ptr;
            if (c == delim)
                break;
        }
        string_array_append(alloc, &arr, (String){
                                .data = first,
                                .len = (int)(ptr - first),
                            });
        ptr++;
    }

    return arr;
}

static void string_print(String str) {
    for (int i = 0; i < str.len; ++i) {
        putc(str.data[i], stdout);
    }
}

static void string_println(String str) {
    string_print(str);
    putc('\n', stdout);
}

static String string_print_buf(char *buf, usize size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = stbsp_vsprintf(buf, fmt, args);
    va_end(args);

    return (String){
        .data = buf,
        .len = len,
    };
}

static String string_print_bufv(char *buf, usize size, const char *fmt, va_list args) {
    int len = stbsp_vsprintf(buf, fmt, args);

    return (String){
        .data = buf,
        .len = len,
    };
}

static String string_printfv(Allocator *alloc, const char *fmt, va_list args) {
    uint buf_size = string_len(fmt) * 2;
    char *buf = (char *)alloc->alloc(alloc, buf_size);
    return string_print_bufv(buf, buf_size, fmt, args);
}

static String string_printf(Allocator *alloc, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String result = string_printfv(alloc, fmt, args);
    va_end(args);

    return result;
}

#endif
