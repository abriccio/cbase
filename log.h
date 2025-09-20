#include <stdio.h>
#include <signal.h>

#ifndef APP_NAME
#define APP_NAME ""
#endif

#define print(fmt, args...) printf("%s: (%s:%d): " fmt, APP_NAME, __func__, __LINE__, ## args, NULL)

#define err(fmt, args...) fprintf(stderr, "%s: (%s:%d): ERROR " fmt, APP_NAME, __func__, __LINE__, ## args, NULL)

#define BREAKPOINT raise(SIGINT)

