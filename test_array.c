#include "data_structs.h"
#include <stdio.h>

int main() {
  printf("=== Testing Dynamic Array ===\n");

  /* Test the lifecycle: create -> add -> print -> free */
  DynamicArray *paths = da_create();
  if (!paths) {
    printf("Failed to create array\n");
    return 1;
  }

  /* Add some test file paths */
  da_append(paths, "./components/Header.jsx");
  da_append(paths, "./modules/auth/components/LoginForm.jsx");
  da_append(paths, "./components/Footer.jsx");

  printf("File paths found:\n");
  da_print(paths);

  da_free(paths);
  printf("Memory cleaned up successfully\n");

  return 0;
}
