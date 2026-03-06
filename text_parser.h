#ifndef TEXT_PARSER_H
#define TEXT_PARSER_H

#include "data_structs.h"
#include "file_reader.h"

/* Parser configuration */
typedef struct {
    size_t max_string_len;  /* Skip strings longer than this */
} ParserConfig;

/* ── Configuration ──────────────────────────────────────────────────── */
ParserConfig *parser_config_create(void);
void          parser_config_free(ParserConfig *config);

/* ── Helpers ────────────────────────────────────────────────────────── */

/* Returns 1 if text is empty or only whitespace */
int is_empty_or_whitespace(const char *text);

/* Returns 1 if str looks like human-readable text:
   at least 3 chars, starts with a letter, and has a space OR uppercase */
int looks_like_human_text(const char *str, size_t len);

/* Returns 1 if the entire line should be skipped:
   comments, next-intl imports, "use client"/"use server", translator init */
int is_safe_line(const char *line);

/* Returns 1 if the string at str_start in *line is the direct first
   argument of a known translator call like t("key") or errors("key") */
int is_inside_translator_call(const char *line, size_t str_start,
                               const DynamicArray *tnames);

/* Scans file content and fills *names with every variable name that was
   assigned via useTranslations() / getTranslations() / useFormatter() */
int collect_translator_names(const char *content, size_t size,
                              DynamicArray *names);

/* ── Pattern detectors (one per category) ───────────────────────────── */
/*  Each returns the number of findings added to *results.
    Result format: "filepath:line:col: <TAG> matched_text"            */

/* Pattern 1 — >plain text< between JSX tags */
int detect_jsx_text_nodes(const char *line, size_t line_num,
                           const char *file_path,
                           const DynamicArray *tnames,
                           DynamicArray *results);

/* Pattern 2 — placeholder/title/aria-label/alt/label/description="literal" */
int detect_string_props(const char *line, size_t line_num,
                        const char *file_path,
                        const DynamicArray *tnames,
                        DynamicArray *results);

/* Pattern 3 — toast.success/error/warning/info/message("literal") */
int detect_toast_literals(const char *line, size_t line_num,
                          const char *file_path,
                          const DynamicArray *tnames,
                          DynamicArray *results);

/* Pattern 4 — setError/setWarning/setMessage/setState("literal") */
int detect_set_state_literals(const char *line, size_t line_num,
                               const char *file_path,
                               const DynamicArray *tnames,
                               DynamicArray *results);

/* Pattern 5 — Zod: message:"literal", required_error:"literal",
                    .min(n,"literal"), .max(n,"literal"), etc. */
int detect_zod_messages(const char *line, size_t line_num,
                        const char *file_path,
                        const DynamicArray *tnames,
                        DynamicArray *results);

/* Pattern 6 — {"literal"} string literal in JSX expression block */
int detect_jsx_expr_strings(const char *line, size_t line_num,
                             const char *file_path,
                             const DynamicArray *tnames,
                             DynamicArray *results);

/* Pattern 7 — throw new Error("literal") */
int detect_throw_errors(const char *line, size_t line_num,
                        const char *file_path,
                        DynamicArray *results);

/* ── Main entry point ───────────────────────────────────────────────── */
int scan_file_for_untranslated(const char *file_path,
                               const FileBuffer *buffer,
                               const ParserConfig *config,
                               DynamicArray *results);

#endif /* TEXT_PARSER_H */