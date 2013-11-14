#ifndef ERROR_H
#define ERROR_H

#include <stdlib.h>

#if DEBUG
	#define DBG(fmt, ...) fprintf(stderr, "debug: " fmt "\n", ##__VA_ARGS__)
#else
	#define DBG(fmt, ...)
#endif

#define ERROR(fmt, ...) fprintf(stderr, "error: " fmt "\n", ##__VA_ARGS__)

#define WARN(fmt, ...) fprintf(stderr, "warn: " fmt "\n", ##__VA_ARGS__)

#define INFO(fmt, ...) fprintf(stderr, "info: " fmt "\n", ##__VA_ARGS__)

#define FATAL(fmt, ...) do { fprintf(stderr, "fatal: [%s@%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); exit(EXIT_FAILURE); } while (0)

#endif
