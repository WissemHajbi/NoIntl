#include "text_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STRING_LENGTH 300 /* Don't flag very long strings */

/* Step 4A: Parser configuration */
ParserConfig *parser_config_create(void) {
  ParserConfig *pc = malloc(sizeof(ParserConfig));
  if (!pc) {
    fprintf(stderr, "Error: malloc failed for ParserConfig\n");
    return NULL;
  }

  /* Initialize configuration */
  pc->max_string_len = MAX_STRING_LENGTH;

  return pc;
}

void parser_config_free(ParserConfig *config) {
  if (config) {
    free(config);
  }
}

/* Step 4B: Detect untranslated JSX text patterns */

/* Check if text is only whitespace/empty */
int is_empty_or_whitespace(const char *text) {
  if (!text || *text == '\0')
    return 1;

  for (size_t i = 0; text[i] != '\0'; i++) {
    if (!isspace((unsigned char)text[i])) {
      return 0; /* Found non-whitespace */
    }
  }
  return 1; /* All whitespace */
}

/* Check if extracted text is actual content (not code artifacts) */
int is_valid_jsx_content(const char *text) {
  if (!text || is_empty_or_whitespace(text))
    return 0;

  /* Reject if contains code patterns */
  if (strchr(text, '{') || strchr(text, '}') || 
      strchr(text, '=') || strchr(text, ';')) {
    return 0; /* Looks like code, not text */
  }

  /* Reject if starts with JSX patterns */
  if (text[0] == ')' || text[0] == ']') {
    return 0; /* Starts with closing bracket */
  }

  return 1; /* Valid plain text content */
}

/* Pattern 1: >plain text< (JSX text node) */
int is_untranslated_jsx_text(const char *text) {
  /* Skip whitespace-only content */
  if (is_empty_or_whitespace(text)) {
    return 0;
  }

  /* If text contains {, it's likely a JSX expression, not plain text */
  if (strchr(text, '{') != NULL) {
    return 0; /* It's an expression, not plain text */
  }

  return 1; /* Plain text → untranslated */
}

/* Pattern 2: {'string'} or {"string"} or {`string`} (string literal in
 * braces)*/
int is_untranslated_string_literal(const char *buffer_content,
                                   size_t string_pos) {
  /*
  Check what comes BEFORE the string:
  - Is it { preceded by {
  - Is it surrounded by quotes without word characters around it

  Examples:
  {'Hello'}       → YES, untranslated
  {t('Hello')}    → NO, inside function call
  {variable}      → NO, not a string literal
  */

  if (!buffer_content || string_pos == 0)
    return 0;

  /* Check character immediately before the string quote */
  char before = buffer_content[string_pos - 1];

  /* If immediately after {, it's a string literal in JSX expression */
  if (before == '{') {
    return 1; /* {'literal'} pattern → untranslated */
  }

  /* If after whitespace followed by {, also a string literal */
  int pos = (int)string_pos - 2;
  while (pos >= 0 && isspace(buffer_content[pos])) {
    pos--;
  }
  if (pos >= 0 && buffer_content[pos] == '{') {
    return 1; /* { 'literal'} pattern → untranslated */
  }

  return 0; /* Not a direct string literal in JSX */
}

/* Step 4C: Extract tag name and text content from JSX */
int extract_jsx_text(const char *content, size_t tag_pos, char *tag_name_out,
                     size_t tag_name_len, char *text_out, size_t text_len) {
  if (!content || !tag_name_out || !text_out)
    return -1;

  size_t pos = tag_pos;

  /* Step 1: Verify we're at < */
  if (content[pos] != '<')
    return -1;
  pos++;

  /* Step 2: Extract tag name until >, space, or / */
  size_t tag_start = pos;
  while (pos < strlen(content) && content[pos] != '>' && content[pos] != ' ' &&
         content[pos] != '/' && !isspace(content[pos])) {
    pos++;
  }

  size_t tag_len = pos - tag_start;
  if (tag_len >= tag_name_len)
    return -1; /* Tag name too long for buffer */

  strncpy(tag_name_out, &content[tag_start], tag_len);
  tag_name_out[tag_len] = '\0'; /* ✅ Null terminator */

  /* Step 3: Find closing > */
  while (pos < strlen(content) && content[pos] != '>') {
    pos++;
  }
  if (pos >= strlen(content))
    return -1; /* No closing > found */

  pos++; /* Move past the > */

  /* Step 4: Extract text between > and < */
  size_t text_start = pos;
  while (pos < strlen(content) && content[pos] != '<') {
    pos++;
  }
  if (pos >= strlen(content))
    return -1; /* No closing < found */

  size_t actual_text_len = pos - text_start;
  
  /* Step 5: Trim leading whitespace */
  while (text_start < pos && isspace((unsigned char)content[text_start])) {
    text_start++;
    actual_text_len--;
  }
  
  /* Step 6: Trim trailing whitespace */
  while (actual_text_len > 0 && isspace((unsigned char)content[text_start + actual_text_len - 1])) {
    actual_text_len--;
  }
  
  /* Skip if only whitespace remains */
  if (actual_text_len == 0) {
    text_out[0] = '\0';
    return 0;  /* Return 0 to indicate whitespace-only content */
  }

  if (actual_text_len >= text_len)
    return -1; /* Text too long for buffer */

  strncpy(text_out, &content[text_start], actual_text_len);
  text_out[actual_text_len] = '\0'; /* ✅ Null terminator */

  return (int)actual_text_len; /* Return actual length extracted */
}

/* Step 4D: Main scanning - find untranslated JSX text nodes */
int scan_file_for_untranslated_jsx(const char *file_path,
                                   const FileBuffer *buffer,
                                   const ParserConfig *config,
                                   DynamicArray *results) {
  /*
  Algorithm:
  1. Validate inputs
  2. Scan through buffer looking for opening tags: <
  3. For each <TagName>, find closing > and extract text
  4. Check if text contains { (JSX expression)
  5. If plain text (no {), check if longer than max_string_len
  6. If NOT too long, add to results
  7. Track line numbers and columns
  8. Find closing </TagName> and continue

  Return: 0 on success, -1 on error
  */
  if (!file_path || !buffer || !buffer->content || !config || !results) {
    fprintf(stderr, "Error: Invalid inputs to scan_file_for_untranslated_jsx\n");
    return -1;
  }

  size_t line = 1;
  size_t column = 1;
  size_t i = 0;

  /* Iterate through entire buffer */
  while (i < buffer->size) {
    /* Track line numbers and columns */
    if (buffer->content[i] == '\n') {
      line++;
      column = 1;
      i++;
      continue;
    }
    column++;

    /* Look for opening tag < */
    if (buffer->content[i] == '<') {
      char tag_name[128];
      char text_content[512];

      /* Try to extract tag name and text content */
      int text_len = extract_jsx_text(buffer->content, i, tag_name, sizeof(tag_name),
                                       text_content, sizeof(text_content));

      if (text_len > 0) {
        /* Check if text is valid content AND untranslated AND within size limit */
        if (is_valid_jsx_content(text_content) &&
            is_untranslated_jsx_text(text_content) &&
            (size_t)text_len <= config->max_string_len) {
          
          /* Format result string */
          char result[1024];
          snprintf(result, sizeof(result), "%s:%zu:%zu: <%s> %s", 
                   file_path, line, column, tag_name, text_content);
          
          /* Add to results array */
          da_append(results, result);
        }

        /* Skip past the closing tag to find next < */
        while (i < buffer->size && buffer->content[i] != '<') {
          if (buffer->content[i] == '\n') {
            line++;
            column = 1;
          } else {
            column++;
          }
          i++;
        }
      } else if (text_len == 0) {
        /* Text was only whitespace - skip normally */
        while (i < buffer->size && buffer->content[i] != '<') {
          if (buffer->content[i] == '\n') {
            line++;
            column = 1;
          } else {
            column++;
          }
          i++;
        }
      } else {
        /* extract_jsx_text failed (-1 returned) - skip this < and continue */
        i++;
      }
    }

    i++;
  }

  return 0;  /* Success */
}
