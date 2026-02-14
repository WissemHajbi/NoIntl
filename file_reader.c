#include "file_reader.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define INITIAL_BUFFER_SIZE 4096         /* 4KB initial buffer */
#define MAX_FILE_SIZE (10 * 1024 * 1024) /* 10MB safety limit */

/* Step 3A: Buffer creation */
FileBuffer *fb_create(size_t initial_capacity) {
  /* Validate input */
  if (initial_capacity == 0) {
    fprintf(stderr, "Error: initial_capacity must be > 0\n");
    return NULL;
  }

  /* Step 1: Allocate FileBuffer structure */
  FileBuffer *fb = malloc(sizeof(FileBuffer));
  if (!fb) {
    fprintf(stderr, "Error: malloc failed for FileBuffer\n");
    return NULL;
  }

  /* Step 2: Allocate content buffer */
  fb->content = malloc(initial_capacity);
  if (!fb->content) {
    fprintf(stderr, "Error: malloc failed for content buffer\n");
    free(fb); /* CRITICAL: cleanup structure on failure */
    return NULL;
  }

  /* Step 3: Initialize size and capacity */
  fb->size = 0;                    /* Buffer is empty, nothing read yet */
  fb->capacity = initial_capacity; /* This is what we allocated */

  return fb;
}

/* Step 3B: Read entire file into buffer */
int fb_read_file(const char *filepath, FileBuffer *buffer) {
  /* Step 1: Validate inputs */
  if (!filepath || !buffer) {
    fprintf(stderr, "Error: NULL filepath or buffer\n");
    return -1;
  }

  /* Step 2: Get file size with stat() */
  struct stat file_stat;
  if (stat(filepath, &file_stat) == -1) {
    perror("stat");
    return -1;
  }

  /* Step 3: Check file size safety limits */
  if (file_stat.st_size > MAX_FILE_SIZE) {
    fprintf(stderr, "Error: File too large (%ld bytes)\n", file_stat.st_size);
    return -1;
  }

  if (file_stat.st_size > (off_t)buffer->capacity) {
    fprintf(stderr, "Error: File larger than buffer capacity\n");
    return -1;
  }

  /* Step 4: Open file for reading */
  FILE *file = fopen(filepath, "rb");
  if (!file) {
    perror("fopen");
    return -1;
  }

  /* Step 5: Read entire file into buffer */
  size_t bytes_read = fread(buffer->content, 1, file_stat.st_size, file);
  if (bytes_read != (size_t)file_stat.st_size) {
    fprintf(stderr,
            "Error: Failed to read entire file (got %zu of %ld bytes)\n",
            bytes_read, file_stat.st_size);
    fclose(file);
    return -1;
  }

  /* Step 6: Update buffer metadata */
  buffer->size = bytes_read; /* How much we actually read */
  /* capacity stays the same - it's what we allocated */

  /* Step 7: Cleanup and return success */
  fclose(file);
  return 0;
}

/* Step 3C: Memory cleanup */
void fb_free(FileBuffer *buffer) {
  if (!buffer)
    return;
  free(buffer->content);
  free(buffer);
}

/* Step 3D: Debug preview */
void fb_print_preview(const FileBuffer *buffer, size_t max_chars) {
  /* Step 1: Validate buffer */
  if (!buffer || !buffer->content || buffer->size == 0) {
    printf("(empty or invalid buffer)\n");
    return;
  }

  /* Step 2: Determine how much to print */
  size_t preview_len = (buffer->size < max_chars) ? buffer->size : max_chars;

  /* Step 3: Print with special character handling */
  for (size_t i = 0; i < preview_len; i++) {
    char c = buffer->content[i];

    if (c == '\n')
      printf("\\n"); // Show newline as \n
    else if (c == '\t')
      printf("\\t"); // Show tab as \t
    else if (c == '\0')
      printf("\\0");                // Show null as \0
    else if (c >= 32 && c <= 126) { // Printable ASCII
      printf("%c", c);
    } else {
      printf("?"); // Non-printable as ?
    }
  }

  /* Step 4: Show truncation indicator if needed */
  if (buffer->size > max_chars) {
    printf(" ... (%zu more bytes)", buffer->size - max_chars);
  }
  printf("\n");
}
