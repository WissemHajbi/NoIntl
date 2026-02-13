# Next.js Internationalization Scanner 🔍

## 🎯 Project Mission

Build a CLI tool in pure C that scans Next.js projects for untranslated static strings. **No magic libraries** - just stdlib, manual memory management, and algorithmic thinking.

## 🏗️ Architecture Overview

```
CLI Tool Structure:
├── main.c           # Entry point, argument parsing
├── data_structs.c   # Dynamic arrays, linked lists
├── directory.c      # File system traversal  
├── file_reader.c    # Buffered file I/O
├── text_parser.c    # String analysis & pattern detection
└── algorithm.c      # Advanced string searching
```

### Compilation
```bash
# Compile with warnings enabled
gcc -Wall -Wextra -std=c99 -o scanner main.c data_structs.c

# Run tests
./scanner --test

# Debug mode
gcc -g -DDEBUG -Wall -o scanner_debug *.c
```

## 🎯 Target Use Case

Scan Next.js projects for untranslated strings:

```bash
# Find hardcoded strings not wrapped in translation functions
./scanner --directory ./src/components --extensions .jsx,.tsx

# Example output:
# ./components/Header.jsx:23: "Welcome to our site" (not in useTranslation)
# ./modules/auth/LoginForm.tsx:45: "Please log in" (not in t() call)
```

---

> **"The best way to learn systems programming is to build something real, make mistakes, understand why they happened, and fix them properly."**

**Built with 💻 and lots of ☕ **
