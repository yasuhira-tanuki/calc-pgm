#ifndef DISPLAY_H
#define DISPLAY_H

#include "value.h"

#define VERSION "1.0"

void print_result(Value v);
void print_help(void);
void print_types(void);
void print_encodings(void);
void print_size(const char *str);
void print_log_result(double x);

#endif /* DISPLAY_H */
