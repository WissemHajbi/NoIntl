#ifndef TEXT_PARSER_H
#define TEXT_PARSER_H

#include "data_structs.h"
#include "file_reader.h"

/* Untranslated JSX text finding result */
typedef struct {
    size_t line_number;     /* Line where found */
    size_t column;          /* Column position */
    char *tag_name;         /* Component name: Text, Button, p, etc */
    char *text_content;     /* The untranslated text */
} UntranslatedJSXText;

/* Parser configuration - minimal for JSX text only */
typedef struct {
    size_t max_string_len;  /* Skip strings longer than this (likely error messages) */
} ParserConfig;

/* Function declarations */
ParserConfig* parser_config_create(void);
void parser_config_free(ParserConfig *config);

/* Core scanning - find untranslated JSX text nodes */
int scan_file_for_untranslated_jsx(const char *file_path, const FileBuffer *buffer, 
                                    const ParserConfig *config, DynamicArray *results);

/* Helper functions */
int extract_jsx_text(const char *content, size_t tag_close_pos,
                     char *tag_name_out, size_t tag_name_len,
                     char *text_out, size_t text_len);
int is_jsx_expression(const char *text);  /* Has { } ? */

#endif