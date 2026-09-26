#ifndef UTIL_H
#define UTIL_H

#if defined(__GNUC__) || defined(__clang__)
#define HAVE_BUILTIN_CLZ 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define ARCH_X86_64 1
#define ARCH_AMD64 1
#elif defined(__i386__) || defined(_M_IX86)
#define ARCH_X86 1
#define ARCH_AMD32 1
#elif defined(__arm__) || defined(_M_ARM)
#define ARCH_ARM32 1
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm64__)
#define ARCH_ARM64 1
#endif

#if ARCH_X86_64 || ARCH_X86
static inline int clz32(unsigned int x) {
	unsigned int res;
	__asm__("bsrl %1, %0" : "=r"(res) : "r"(x));
	return (int)res;
}

static inline int clz64(unsigned long long x) {
	unsigned long long res;
	__asm__("bsrq %1, %0" : "=r"(res) : "r"(x));
	return (int)res;
}

#elif ARCH_ARM64 || ARCH_ARM32

static inline int clz32(unsigned int x) {
	unsigned int res;
	__asm__("clz %0, %1" : "=r"(res) : "r"(x));
	return (int)res;
}

static inline int clz64(unsigned long long x) {
	unsigned long long res;
	__asm__("clz %0, %1" : "=r"(res) : "r"(x));
	return (int)res;
}

#endif

#if HAVE_BUILTIN_CLZ
#define clz32 __builtin_clz
#define clz64 __builtin_clzll
#endif

#endif
