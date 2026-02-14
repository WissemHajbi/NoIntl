#include "data_structs.h"
#include <stdio.h>
#include <string.h>

#define INITIAL_CAPACITY 8

/* TODO: We'll implement these functions step by step */

DynamicArray* da_create(void) {
    /* Step 1: Allocate the array structure itself */
    DynamicArray *da = malloc(sizeof(DynamicArray));
    if (!da) {
        return NULL;
    }

    /* Step 2: Initialize the internal string array */
    da->strings = malloc(INITIAL_CAPACITY * sizeof(char*));
    if (!da->strings) {
        free(da);  /* CRITICAL: cleanup the first allocation! */
        return NULL;
    }

    /* Step 3: Set initial size and capacity */
    da->size = 0;
    da->capacity = INITIAL_CAPACITY;

    return da;
}

int da_append(DynamicArray *arr, const char *str) {
    /* Step 1: Check if we need to grow the array */
    if (arr->size >= arr->capacity) {
        size_t new_capacity = arr->capacity * 2;
        char **new_strings = realloc(arr->strings, new_capacity * sizeof(char*));
        if (!new_strings) {
            return -1;  /* Growth failed, original array still intact */
        }
        arr->strings = new_strings;
        arr->capacity = new_capacity;
    }
    
    /* Step 2: Allocate memory for the new string */
    size_t str_len = strlen(str) + 1;  /* +1 for null terminator */
    char *str_copy = malloc(str_len);
    if (!str_copy) {
        return -1;  /* String allocation failed */
    }
    
    /* Step 3: Copy the string content */
    strcpy(str_copy, str);
    
    /* Step 4: Add pointer to our array and increment size */
    arr->strings[arr->size] = str_copy;
    arr->size++;
    
    return 0;  /* Success */
}

void da_free(DynamicArray *arr) {
    if (!arr) return;
    /* Step 1: Free each individual string */
    for(size_t i = 0; i < arr->size; i++)
    {
      free(arr->strings[i]);
    }
    /* Step 2: Free the array of pointers */
   free(arr->strings);
    /* Step 3: Free the structure itself */
  free(arr);
}

void da_print(const DynamicArray *arr) {
    if (!arr) {
        printf("Array is NULL\n");
        return;
    }
    
    printf("Dynamic Array:\n");
    printf("  Size: %zu\n", arr->size);
    printf("  Capacity: %zu\n", arr->capacity);
    printf("  Contents:\n");
    
    if (arr->size == 0) {
        printf("    (empty)\n");
    } else {
        for (size_t i = 0; i < arr->size; i++) {
            printf("    [%zu]: \"%s\"\n", i, arr->strings[i]);
        }
    }
}
