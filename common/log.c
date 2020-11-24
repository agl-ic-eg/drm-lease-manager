#include "log.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

static bool debug_log = false;

void dlm_log_enable_debug(bool enable)
{
	debug_log = enable;
}

void dlm_log_print(bool debug, FILE *stream, char *fmt, ...)
{
	if (debug && !debug_log)
		return;

	va_list argl;
	va_start(argl, fmt);
	vfprintf(stream, fmt, argl);
	va_end(argl);
}
