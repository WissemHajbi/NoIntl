#ifndef DATA_STRUCTS_H
#define DATA_STRUCTS_H

#include <stdlib.h>

/* Dynamic String Array - our foundational data structure */
typedef struct {
  char **strings;  /* Array of string pointers */
  size_t size;     /* Current number of strings */
  size_t capacity; /* Total allocated space */
} DynamicArray;

/* Function declarations */
DynamicArray *da_create(void);
int da_append(DynamicArray *arr, const char *str);
void da_free(DynamicArray *arr);
void da_print(const DynamicArray *arr);

#endif
