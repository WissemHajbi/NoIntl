#include "directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define INITIAL_EXT_CAPACITY 8

/* Extension filter management - Step 2A */
ExtensionFilter* ext_filter_create(void) {
    ExtensionFilter *filter = malloc(sizeof(ExtensionFilter));
    if (!filter) return NULL;

    filter->extensions = malloc(INITIAL_EXT_CAPACITY * sizeof(char*));
    if (!filter->extensions) {
        free(filter);
        return NULL;
    }
    
    filter->count = 0;
    return filter;
}

int ext_filter_add(ExtensionFilter *filter, const char *extension) {
    if (!filter || !extension) return -1;  // Handle NULL inputs
    
    // Step 1: Check if we have space (fixed size for now)
    if (filter->count >= INITIAL_EXT_CAPACITY) {
        return -1;  // No space available
    }
    
    // Step 2: Duplicate the extension string
    char *ext_copy = malloc(strlen(extension) + 1);
    if (!ext_copy) {
        return -1;  // Allocation failed
    }
    strcpy(ext_copy, extension);
    
    // Step 3: Add to array and increment count
    filter->extensions[filter->count] = ext_copy;
    filter->count++;
    
    return 0;  // Success (return 0, not count)
}

void ext_filter_free(ExtensionFilter *filter) {
    if (!filter) return;
    
    /* Step 1: Free each individual extension string */
    for (size_t i = 0; i < filter->count; i++) {
        free(filter->extensions[i]);
    }
    
    /* Step 2: Free the extensions array */
    free(filter->extensions);
    
    /* Step 3: Free the structure itself */
    free(filter);
}

int ext_filter_matches(const ExtensionFilter *filter, const char *filename) {
    if (!filter || !filename) return -1;  /* Null safety - no match */
    
    size_t filelen = strlen(filename);
    
    /* Check each extension in the filter */
    for (size_t i = 0; i < filter->count; i++) {
        size_t extlen = strlen(filter->extensions[i]);
        
        /* Extension can't be longer than filename */
        if (filelen < extlen) continue;
        
        /* Get the last N characters of filename */
        const char *file_end = filename + filelen - extlen;
        
        /* Compare strings */
        if (strcmp(file_end, filter->extensions[i]) == 0) {
            return (int)i;  /* Return the index of matching extension */
        }
    }
    
    return -1;  /* No match found */
}

#include <limits.h>
#include <dirent.h>

int scan_directory_recursive(const char *path, const ScanConfig *config, DynamicArray *results) {
    if (!path || !config || !results) return -1;

    /* Open directory */
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. entries */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Construct full path to entry */
        char full_path[PATH_MAX];
        int ret = snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (ret < 0 || ret >= (int)sizeof(full_path)) {
            continue;
        }

        /* Get file information */
        struct stat file_stat;
        if (stat(full_path, &file_stat) == -1) {
            continue;
        }

        /* If regular file: check extension and add if match */
        if (S_ISREG(file_stat.st_mode)) {
            if (ext_filter_matches(config->filter, entry->d_name) >= 0) {
                da_append(results, full_path);
            }
        }
        /* If directory: recurse into it */
        else if (S_ISDIR(file_stat.st_mode)) {
            if (config->follow_symlinks || !S_ISLNK(file_stat.st_mode)) {
                scan_directory_recursive(full_path, config, results);
            }
        }
    }

    closedir(dir);
    return 0;
}

int collect_target_files(const ScanConfig *config, DynamicArray *file_paths) {
    /* Step 1: Validate all inputs */
    if (!config || !config->base_path || !config->filter || !file_paths) {
        fprintf(stderr, "Error: Invalid configuration parameters\n");
        return -1;
    }

    /* Step 2: Check if base_path exists and is a directory */
    struct stat base_stat;
    if (stat(config->base_path, &base_stat) == -1) {
        perror("stat");
        return -1;
    }

    /* Step 3: Verify it's actually a directory */
    if (!S_ISDIR(base_stat.st_mode)) {
        fprintf(stderr, "Error: %s is not a directory\n", config->base_path);
        return -1;
    }

    /* Step 4: Call recursive scanner to collect files */
    if (scan_directory_recursive(config->base_path, config, file_paths) == -1) {
        fprintf(stderr, "Error: Failed to scan directory recursively\n");
        return -1;
    }

    /* Step 5: Success */
    return 0;
}
