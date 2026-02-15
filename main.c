#include "data_structs.h"
#include "directory.h"
#include "file_reader.h"
#include "text_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_BUFFER_SIZE 4096 /* 4KB initial buffer */

int main(int argc, char *argv[]) {
  /* Step 1: Validate arguments */
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
    return 1;
  }

  const char *base_dir = argv[1];

  /* Step 2: Create extension filter */
  ExtensionFilter *filter = ext_filter_create();
  if (!filter) {
    fprintf(stderr, "Error: Failed to create extension filter\n");
    return 1;
  }
  ext_filter_add(filter, ".tsx");
  ext_filter_add(filter, ".jsx");

  /* Step 3: Create scan configuration */
  ScanConfig scan_cfg = {.base_path = base_dir,
                         .filter = filter,
                         .max_depth = -1,
                         .follow_symlinks = 0};

  /* Step 4: Scan for target files */
  DynamicArray *file_paths = da_create();
  if (!file_paths) {
    fprintf(stderr, "Error: Failed to create file paths array\n");
    ext_filter_free(filter);
    return 1;
  }

  if (collect_target_files(&scan_cfg, file_paths) == -1) {
    fprintf(stderr, "Error: Failed to scan directory\n");
    da_free(file_paths);
    ext_filter_free(filter);
    return 1;
  }

  if (file_paths->size == 0) {
    printf("No .tsx or .jsx files found\n");
    da_free(file_paths);
    ext_filter_free(filter);
    return 0;
  }

  /* Step 5: Process each file and track files with issues */
  DynamicArray *problematic_files = da_create();
  if (!problematic_files) {
    da_free(file_paths);
    ext_filter_free(filter);
    return 1;
  }

  for (size_t i = 0; i < file_paths->size; i++) {
    /* Read file */
    FileBuffer *buf = fb_create(INITIAL_BUFFER_SIZE);
    if (!buf) {
      continue;
    }

    if (fb_read_file(file_paths->strings[i], buf) == -1) {
      fb_free(buf);
      continue;
    }

    /* Scan file for untranslated text */
    ParserConfig *parser_cfg = parser_config_create();
    if (parser_cfg) {
      scan_file_for_untranslated_jsx(file_paths->strings[i], buf, parser_cfg,
                                     problematic_files);
      parser_config_free(parser_cfg);
    }

    /* Cleanup per-file resources */
    fb_free(buf);
  }

  /* Step 6: Print results - grouped by file */
  if (problematic_files->size == 0) {
    printf("✓ All files are properly translated!\n");
  } else {
    printf("✗ Files with untranslated text:\n\n");
    
    const char *current_file = NULL;
    
    for (size_t i = 0; i < problematic_files->size; i++) {
      char *result = problematic_files->strings[i];
      
      /* Parse: filename:line:col: <tag> text_with_newlines */
      char *colon1 = strchr(result, ':');
      if (!colon1) continue;
      
      /* Extract filename */
      size_t filename_len = colon1 - result;
      char filename[512];
      strncpy(filename, result, filename_len);
      filename[filename_len] = '\0';
      
      /* Extract line number (between first and second colon) */
      char *colon2 = strchr(colon1 + 1, ':');
      if (!colon2) continue;
      char line_str[32];
      strncpy(line_str, colon1 + 1, colon2 - colon1 - 1);
      line_str[colon2 - colon1 - 1] = '\0';
      int line_num = atoi(line_str);
      
      /* Extract text (after "> ") */
      char *text_start = strchr(colon2, '>');
      if (!text_start) continue;
      text_start += 2;  /* Skip "> " */
      
      /* Print file header if new file */
      if (!current_file || strcmp(current_file, filename) != 0) {
        printf("  - %s\n", filename);
        current_file = filename;
      }
      
      /* Print the issue */
      printf("      Line %d: \"%s\"\n", line_num, text_start);
    }
    
    printf("\nTotal issues found: %zu\n", problematic_files->size);
  }

  /* Step 7: Cleanup all resources */
  da_free(file_paths);
  da_free(problematic_files);
  ext_filter_free(filter);

  return 0;
}
