#ifndef FILE_READER_H
#define FILE_READER_H

#include <stdlib.h>

/* File buffer structure for reading file contents */
typedef struct {
  char *content;   /* The actual file content */
  size_t size;     /* Number of bytes read */
  size_t capacity; /* Allocated buffer space */
} FileBuffer;

/* File reading configuration */
typedef struct {
  size_t initial_capacity; /* Starting buffer size */
  size_t max_file_size;    /* Maximum file size to read (safety limit) */
  int follow_symlinks;     /* Follow symbolic links? */
} FileReadConfig;

/* Function declarations */
FileBuffer *fb_create(size_t initial_capacity);
int fb_read_file(const char *filepath, FileBuffer *buffer);
void fb_free(FileBuffer *buffer);
void fb_print_preview(const FileBuffer *buffer, size_t max_chars);

#endif
