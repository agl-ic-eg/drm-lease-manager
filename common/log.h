#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include <stdio.h>

#define DEBUG_LOG(FMT, ...) \
	dlm_log_print(true, stdout, "DEBUG: %s: " FMT, __func__, ##__VA_ARGS__)
#define INFO_LOG(FMT, ...) \
	dlm_log_print(false, stdout, "INFO: " FMT, ##__VA_ARGS__)
#define WARN_LOG(FMT, ...) \
	dlm_log_print(false, stderr, "WARNING: " FMT, ##__VA_ARGS__)
#define ERROR_LOG(FMT, ...) \
	dlm_log_print(false, stderr, "ERROR: " FMT, ##__VA_ARGS__)

void dlm_log_enable_debug(bool enable);
void dlm_log_print(bool debug, FILE *stream, char *fmt, ...);
#endif
