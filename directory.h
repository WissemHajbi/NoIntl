#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "data_structs.h"

/* File extension filter structure */
typedef struct {
    char **extensions;    /* Array of extensions: ".jsx", ".tsx" */
    size_t count;        /* Number of extensions */
} ExtensionFilter;

/* Directory scanning configuration */
typedef struct {
    const char *base_path;        /* Starting directory */
    ExtensionFilter *filter;      /* File extensions to include */
    int max_depth;               /* Recursion limit (-1 = unlimited) */
    int follow_symlinks;         /* Follow symbolic links? */
} ScanConfig;

/* Function declarations */
ExtensionFilter* ext_filter_create(void);
int ext_filter_add(ExtensionFilter *filter, const char *extension);
void ext_filter_free(ExtensionFilter *filter);
int ext_filter_matches(const ExtensionFilter *filter, const char *filename);

/* Main directory scanning functions */
int scan_directory_recursive(const char *path, const ScanConfig *config, DynamicArray *results);
int collect_target_files(const ScanConfig *config, DynamicArray *file_paths);

#endif