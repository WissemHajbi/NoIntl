#include "data_structs.h"
#include "directory.h"
#include "file_reader.h"
#include "text_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_BUFFER_SIZE 4096 /* 4KB initial buffer */

void display_banner() {
  printf("\n");
  printf("     _   _  ____  ___ _   _ _____ _                          \n");
  printf("    |  \\| |/ __ \\|_ _| \\ | |_   _| |                         \n");
  printf("    |  \\| | |  | | | |  \\| | | | | |                         \n");
  printf("    | . ` | |  | | | | . ` | | | | |                         \n");
  printf("    | |\\  | |__| |_|_| |\\  | | | | |___                      \n");
  printf("    |_| \\_|\\____/|___|_| \\_| |_| |_____|                     \n");
  printf("                                                              \n");
  printf("                                        < by Wissem Hajbi >   \n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  /* Display project banner */
  display_banner();

  /* Step 1: Validate arguments */
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
    return 1;
  }

  const char *base_dir = argv[1];

  /* Step 2: Create extension filter — scan .tsx, .jsx, .ts, .js */
  ExtensionFilter *filter = ext_filter_create();
  if (!filter) {
    fprintf(stderr, "Error: Failed to create extension filter\n");
    return 1;
  }
  ext_filter_add(filter, ".tsx");
  ext_filter_add(filter, ".jsx");
  ext_filter_add(filter, ".ts");
  ext_filter_add(filter, ".js");

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
    printf("No .tsx / .jsx / .ts / .js files found\n");
    da_free(file_paths);
    ext_filter_free(filter);
    return 0;
  }

  printf("Scanning %zu file(s)...\n\n", file_paths->size);

  /* Step 5: Process each file and collect issues */
  DynamicArray *problematic_files = da_create();
  if (!problematic_files) {
    da_free(file_paths);
    ext_filter_free(filter);
    return 1;
  }

  for (size_t i = 0; i < file_paths->size; i++) {
    FileBuffer *buf = fb_create(INITIAL_BUFFER_SIZE);
    if (!buf)
      continue;

    if (fb_read_file(file_paths->strings[i], buf) == -1) {
      fb_free(buf);
      continue;
    }

    ParserConfig *parser_cfg = parser_config_create();
    if (parser_cfg) {
      scan_file_for_untranslated(file_paths->strings[i], buf, parser_cfg,
                                 problematic_files);
      parser_config_free(parser_cfg);
    }

    fb_free(buf);
  }

  /* Step 6: Print results grouped by file */
  if (problematic_files->size == 0) {
    printf("✓ All files are properly translated!\n");
  } else {
    printf("✗ Files with untranslated text:\n\n");

    char current_file[512] = "";

    for (size_t i = 0; i < problematic_files->size; i++) {
      char *result = problematic_files->strings[i];

      /*  Result format: "filepath:line:col: <TAG> text"
          We anchor on the ": <" sequence to safely handle Windows paths
          that contain a leading drive letter colon (C:\...).          */
      char *tag_marker = strstr(result, ": <");
      if (!tag_marker)
        continue;

      /* Walk backwards from tag_marker to find line and file separators */
      char *last_colon = tag_marker - 1;
      while (last_colon > result && *last_colon != ':')
        last_colon--;
      if (last_colon <= result)
        continue;

      char *prev_colon = last_colon - 1;
      while (prev_colon > result && *prev_colon != ':')
        prev_colon--;
      if (prev_colon <= result)
        continue;

      /* Filename: everything before prev_colon */
      size_t filename_len = (size_t)(prev_colon - result);
      char filename[512];
      if (filename_len >= sizeof(filename))
        continue;
      strncpy(filename, result, filename_len);
      filename[filename_len] = '\0';

      /* Line number: between prev_colon+1 and last_colon */
      size_t ln_len = (size_t)(last_colon - prev_colon - 1);
      char line_str[32];
      if (ln_len >= sizeof(line_str))
        continue;
      strncpy(line_str, prev_colon + 1, ln_len);
      line_str[ln_len] = '\0';
      int line_num = atoi(line_str);

      /* Tag: between '<' and '>' after the ": <" anchor */
      char *lt = tag_marker + 2; /* points at '<' */
      char *gt = strchr(lt, '>');
      if (!gt)
        continue;

      char tag_display[64] = "";
      size_t tlen = (size_t)(gt - lt - 1);
      if (tlen > 0 && tlen < sizeof(tag_display)) {
        strncpy(tag_display, lt + 1, tlen);
        tag_display[tlen] = '\0';
      }

      /* Text: after "> " */
      char *text_start = gt + 2;

      /* Print file header when we encounter a new file */
      if (strcmp(current_file, filename) != 0) {
        printf("  \xE2\x96\xB8 %s\n", filename);
        strncpy(current_file, filename, sizeof(current_file) - 1);
        current_file[sizeof(current_file) - 1] = '\0';
      }

      printf("      Line %-4d  [%-20s]  \"%s\"\n", line_num, tag_display,
             text_start);
    }

    printf("\nTotal issues found: %zu\n", problematic_files->size);
  }

  /* Step 7: Cleanup */
  da_free(file_paths);
  da_free(problematic_files);
  ext_filter_free(filter);

  return 0;
}
