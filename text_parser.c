#include "text_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STRING_LENGTH 300
#define MAX_LINE_LENGTH 4096

/* =====================================================================
   PARSER CONFIGURATION
   ===================================================================== */

ParserConfig *parser_config_create(void) {
  ParserConfig *pc = malloc(sizeof(ParserConfig));
  if (!pc) {
    fprintf(stderr, "Error: malloc failed for ParserConfig\n");
    return NULL;
  }
  pc->max_string_len = MAX_STRING_LENGTH;
  return pc;
}

void parser_config_free(ParserConfig *config) {
  if (config)
    free(config);
}

/* =====================================================================
   HELPERS
   ===================================================================== */

int is_empty_or_whitespace(const char *text) {
  if (!text || *text == '\0')
    return 1;
  for (size_t i = 0; text[i] != '\0'; i++) {
    if (!isspace((unsigned char)text[i]))
      return 0;
  }
  return 1;
}

/* Returns 1 if str looks like human-readable text:
   - at least 3 chars
   - starts with a letter
   - has at least one space OR one uppercase letter                     */
int looks_like_human_text(const char *str, size_t len) {
  if (!str || len < 3)
    return 0;
  if (!isalpha((unsigned char)str[0]))
    return 0;
  int has_space = 0, has_upper = 0;
  for (size_t i = 0; i < len; i++) {
    if (str[i] == ' ')
      has_space = 1;
    if (isupper((unsigned char)str[i]))
      has_upper = 1;
    if (has_space && has_upper)
      break;
  }
  return has_space || has_upper;
}

/* Returns 1 if this line should be skipped entirely:
   comments, next-intl imports, directives, translator init lines      */
int is_safe_line(const char *line) {
  const char *p = line;
  while (*p && isspace((unsigned char)*p))
    p++;
  if (*p == '\0')
    return 1;

  /* Single-line comments */
  if (p[0] == '/' && p[1] == '/')
    return 1;

  /* Block-comment continuation lines */
  if (p[0] == '*')
    return 1;

  /* Import from next-intl */
  if (strncmp(p, "import", 6) == 0 &&
      (strstr(line, "next-intl") || strstr(line, "next/intl")))
    return 1;

  /* "use client" / "use server" directives */
  if (strstr(p, "\"use client\"") || strstr(p, "'use client'") ||
      strstr(p, "\"use server\"") || strstr(p, "'use server'"))
    return 1;

  /* Translator initialisation: const t = useTranslations(...) */
  if ((strstr(p, "useTranslations(") || strstr(p, "getTranslations(")) &&
      strstr(p, "const ") && strchr(p, '='))
    return 1;

  return 0;
}

/* Returns 1 if the string whose opening quote is at str_start in *line
   is the direct first argument of a known translator call.
   e.g. t("key")  →  safe; setError("msg")  →  not safe.             */
int is_inside_translator_call(const char *line, size_t str_start,
                              const DynamicArray *tnames) {
  if (!tnames || tnames->size == 0 || str_start == 0)
    return 0;

  /* Walk backwards past the opening quote to find the preceding '(' */
  int pos = (int)str_start - 1;
  while (pos >= 0 && isspace((unsigned char)line[pos]))
    pos--;
  if (pos < 0 || line[pos] != '(')
    return 0;
  pos--;
  while (pos >= 0 && isspace((unsigned char)line[pos]))
    pos--;
  if (pos < 0)
    return 0;

  /* Extract the function / method name before '(' */
  int name_end = pos;
  while (pos >= 0 && (isalnum((unsigned char)line[pos]) || line[pos] == '_'))
    pos--;
  int name_start = pos + 1;
  int name_len = name_end - name_start + 1;
  if (name_len <= 0 || name_len >= 64)
    return 0;

  char name[64];
  strncpy(name, &line[name_start], (size_t)name_len);
  name[name_len] = '\0';

  for (size_t i = 0; i < tnames->size; i++) {
    if (strcmp(tnames->strings[i], name) == 0)
      return 1;
  }
  return 0;
}

/* Fills *names with every variable assigned via
   useTranslations() / getTranslations() / useFormatter()              */
int collect_translator_names(const char *content, size_t size,
                             DynamicArray *names) {
  if (!content || !names)
    return -1;
  const char *end = content + size;
  const char *pos = content;

  while (pos < end) {
    const char *use_t = strstr(pos, "useTranslations(");
    const char *get_t = strstr(pos, "getTranslations(");
    const char *use_f = strstr(pos, "useFormatter(");

    const char *found = NULL;
    if (use_t)
      found = use_t;
    if (get_t && (!found || get_t < found))
      found = get_t;
    if (use_f && (!found || use_f < found))
      found = use_f;
    if (!found)
      break;

    /* Expect:  const <NAME> = useTranslations(  */
    const char *back = found - 1;
    while (back > content && isspace((unsigned char)*back))
      back--;
    if (back <= content || *back != '=') {
      pos = found + 1;
      continue;
    }
    back--;
    while (back > content && isspace((unsigned char)*back))
      back--;

    const char *name_end = back + 1;
    while (back > content && (isalnum((unsigned char)*back) || *back == '_'))
      back--;
    const char *name_start = back + 1;
    size_t name_len = (size_t)(name_end - name_start);

    if (name_len >= 1 && name_len < 64) {
      char buf[64];
      strncpy(buf, name_start, name_len);
      buf[name_len] = '\0';
      /* Skip JS keywords that could appear before '=' */
      if (strcmp(buf, "const") != 0 && strcmp(buf, "let") != 0 &&
          strcmp(buf, "var") != 0) {
        da_append(names, buf);
      }
    }
    pos = found + 1;
  }
  return 0;
}

/* =====================================================================
   INTERNAL UTILITY
   ===================================================================== */

/* Appends one result entry.
   Format: "filepath:line:col: <TAG> text"                            */
static void add_result(DynamicArray *results, const char *file_path,
                       size_t line_num, size_t col, const char *tag,
                       const char *text) {
  char result[1024];
  snprintf(result, sizeof(result), "%s:%zu:%zu: <%s> %s", file_path, line_num,
           col, tag, text);
  da_append(results, result);
}

/* Reads a "..." or '...' string starting at *pp (which must point at
   the opening quote).  Fills val/val_len, advances *pp past the
   closing quote.  Returns 1 on success, 0 on failure.               */
static int read_quoted_string(const char **pp, char *val, size_t val_max,
                              size_t *val_len_out) {
  const char *p = *pp;
  if (*p != '"' && *p != '\'')
    return 0;
  char q = *p++;
  const char *start = p;
  while (*p && *p != q) {
    if (*p == '\\')
      p++; /* skip escape character */
    if (*p)
      p++;
  }
  if (!*p)
    return 0; /* unclosed string */
  size_t vlen = (size_t)(p - start);
  if (vlen >= val_max)
    return 0;
  strncpy(val, start, vlen);
  val[vlen] = '\0';
  if (val_len_out)
    *val_len_out = vlen;
  *pp = p + 1; /* skip closing quote */
  return 1;
}

/* =====================================================================
   PATTERN 1 — JSX text nodes:  >plain text<
   ===================================================================== */
int detect_jsx_text_nodes(const char *line, size_t line_num,
                          const char *file_path, const DynamicArray *tnames,
                          DynamicArray *results) {
  (void)tnames; /* JSX text nodes need no translator-call check */
  if (!line)
    return 0;
  size_t len = strlen(line);
  int count = 0;

  for (size_t i = 0; i < len; i++) {
    if (line[i] != '>')
      continue;

    /* Collect raw text until the next '<' */
    size_t ts = i + 1, te = ts;
    while (te < len && line[te] != '<')
      te++;
    if (te >= len)
      continue;

    /* Trim surrounding whitespace */
    size_t a = ts, b = te;
    while (a < b && isspace((unsigned char)line[a]))
      a++;
    while (b > a && isspace((unsigned char)line[b - 1]))
      b--;
    size_t tlen = b - a;
    if (tlen < 3 || tlen >= 256)
      continue;

    char text[256];
    strncpy(text, &line[a], tlen);
    text[tlen] = '\0';

    /* Skip if it contains code characters */
    if (strchr(text, '{') || strchr(text, '}') || strchr(text, '=') ||
        strchr(text, ';') || strchr(text, '(') || strchr(text, ')'))
      continue;

    if (!looks_like_human_text(text, tlen))
      continue;

    /* Extract the opening tag name by scanning backwards */
    char tag[64] = "jsx";
    int j = (int)i - 1;
    while (j >= 0 && line[j] != '<')
      j--;
    if (j >= 0) {
      int tn_s = j + 1, tn_e = tn_s;
      while (tn_e < (int)i && !isspace((unsigned char)line[tn_e]) &&
             line[tn_e] != '>' && line[tn_e] != '/')
        tn_e++;
      int tnl = tn_e - tn_s;
      if (tnl > 0 && tnl < 64) {
        strncpy(tag, &line[tn_s], (size_t)tnl);
        tag[tnl] = '\0';
      }
    }
    /* Skip closing tags and HTML comment markers */
    if (tag[0] == '/' || tag[0] == '!' || tag[0] == '\0')
      continue;

    add_result(results, file_path, line_num, a + 1, tag, text);
    count++;
    i = te; /* jump past this text segment */
  }
  return count;
}

/* =====================================================================
   PATTERN 2 — String props:
   placeholder / aria-label / title / alt / label / description / ...
   ===================================================================== */
static const char *FLAGGED_PROPS[] = {
    "placeholder",  "aria-label",       "title", "alt",          "label",
    "description",  "tooltip",          "hint",  "errorMessage", "helperText",
    "emptyMessage", "noResultsMessage", NULL};

int detect_string_props(const char *line, size_t line_num,
                        const char *file_path, const DynamicArray *tnames,
                        DynamicArray *results) {
  if (!line)
    return 0;
  int count = 0;

  for (int pi = 0; FLAGGED_PROPS[pi]; pi++) {
    const char *prop = FLAGGED_PROPS[pi];
    size_t prop_len = strlen(prop);
    const char *p = line;

    while ((p = strstr(p, prop)) != NULL) {
      size_t prop_pos = (size_t)(p - line);
      p += prop_len;

      /* Word-boundary before prop: must not be alnum, '_', or '-'
         (handles aria-label vs label, data-title vs title, etc.) */
      if (prop_pos > 0) {
        char before = line[prop_pos - 1];
        if (isalnum((unsigned char)before) || before == '_' || before == '-')
          continue;
      }
      /* Word-boundary after prop */
      if (*p && isalnum((unsigned char)*p))
        continue;

      /* Skip whitespace then expect '=' */
      const char *q = p;
      while (*q && isspace((unsigned char)*q))
        q++;
      if (*q != '=')
        continue;
      q++;
      while (*q && isspace((unsigned char)*q))
        q++;

      /* Value must be a raw string literal, not a JSX expression {} */
      if (*q != '"' && *q != '\'')
        continue;

      size_t val_start_pos = (size_t)(q - line);
      char val[256];
      size_t vlen;
      if (!read_quoted_string(&q, val, sizeof(val), &vlen))
        continue;
      if (!looks_like_human_text(val, vlen))
        continue;
      if (is_inside_translator_call(line, val_start_pos + 1, tnames))
        continue;

      char tag[80];
      snprintf(tag, sizeof(tag), "prop:%s", prop);
      add_result(results, file_path, line_num, val_start_pos + 2, tag, val);
      count++;
    }
  }
  return count;
}

/* =====================================================================
   PATTERN 3 — toast.success/error/warning/info/message("literal")
   ===================================================================== */
static const char *TOAST_METHODS[] = {"success", "error",   "warning",
                                      "info",    "message", NULL};

int detect_toast_literals(const char *line, size_t line_num,
                          const char *file_path, const DynamicArray *tnames,
                          DynamicArray *results) {
  if (!line)
    return 0;
  int count = 0;

  for (int mi = 0; TOAST_METHODS[mi]; mi++) {
    char pattern[32];
    const char *p = line;
    snprintf(pattern, sizeof(pattern), "toast.%s", TOAST_METHODS[mi]);

    while ((p = strstr(p, pattern)) != NULL) {
      p += strlen(pattern);
      while (*p && isspace((unsigned char)*p))
        p++;
      if (*p != '(')
        continue;
      p++;
      while (*p && isspace((unsigned char)*p))
        p++;

      /* First argument must be a raw string literal */
      if (*p != '"' && *p != '\'')
        continue;

      size_t val_start_pos = (size_t)(p - line);
      char val[256];
      size_t vlen;
      if (!read_quoted_string(&p, val, sizeof(val), &vlen))
        continue;
      if (!looks_like_human_text(val, vlen))
        continue;
      if (is_inside_translator_call(line, val_start_pos + 1, tnames))
        continue;

      add_result(results, file_path, line_num, val_start_pos + 2, "toast", val);
      count++;
    }
  }

  /* toast({ message: "literal" }) */
  const char *p = strstr(line, "toast(");
  if (p) {
    const char *msg = strstr(p, "message:");
    if (msg) {
      msg += strlen("message:");
      while (*msg && isspace((unsigned char)*msg))
        msg++;
      if (*msg == '"' || *msg == '\'') {
        size_t val_start_pos = (size_t)(msg - line);
        char val[256];
        size_t vlen;
        if (read_quoted_string(&msg, val, sizeof(val), &vlen) &&
            looks_like_human_text(val, vlen) &&
            !is_inside_translator_call(line, val_start_pos + 1, tnames)) {
          add_result(results, file_path, line_num, val_start_pos + 2, "toast",
                     val);
          count++;
        }
      }
    }
  }
  return count;
}

/* =====================================================================
   PATTERN 4 — setError / setWarning / setMessage / setState("literal")
   ===================================================================== */
static const char *SET_FUNCS[] = {"setError",       "setWarning", "setMessage",
                                  "setSuccess",     "setInfo",    "setTitle",
                                  "setDescription", NULL};

int detect_set_state_literals(const char *line, size_t line_num,
                              const char *file_path, const DynamicArray *tnames,
                              DynamicArray *results) {
  if (!line)
    return 0;
  int count = 0;

  for (int fi = 0; SET_FUNCS[fi]; fi++) {
    const char *func = SET_FUNCS[fi];
    size_t func_len = strlen(func);
    const char *p = line;

    while ((p = strstr(p, func)) != NULL) {
      size_t func_pos = (size_t)(p - line);
      p += func_len;

      /* Word-boundary before name */
      if (func_pos > 0 && (isalnum((unsigned char)line[func_pos - 1]) ||
                           line[func_pos - 1] == '_'))
        continue;

      /* Must be followed by '(' */
      const char *q = p;
      while (*q && isspace((unsigned char)*q))
        q++;
      if (*q != '(')
        continue;
      q++;
      while (*q && isspace((unsigned char)*q))
        q++;

      if (*q != '"' && *q != '\'')
        continue;

      size_t val_start_pos = (size_t)(q - line);
      char val[256];
      size_t vlen;
      if (!read_quoted_string(&q, val, sizeof(val), &vlen))
        continue;
      if (!looks_like_human_text(val, vlen))
        continue;
      if (is_inside_translator_call(line, val_start_pos + 1, tnames))
        continue;

      add_result(results, file_path, line_num, val_start_pos + 2, "set-state",
                 val);
      count++;
    }
  }
  return count;
}

/* =====================================================================
   PATTERN 5 — Zod schema messages
   message:"literal", required_error:"literal", .min(n,"literal"), ...
   ===================================================================== */
static const char *ZOD_KEYS[] = {
    "message:", "required_error:", "invalid_type_error:", NULL};
/* Two-arg validators: first arg is a value (number/regex), second is message */
static const char *ZOD_TWO_ARG_METHODS[] = {".min(", ".max(", ".length(",
                                            ".refine(", NULL};
/* Single-arg validators: first (and only) arg is the message */
static const char *ZOD_ONE_ARG_METHODS[] = {
    ".email(", ".url(", ".uuid(", ".cuid(", ".datetime(", ".ip(", NULL};

int detect_zod_messages(const char *line, size_t line_num,
                        const char *file_path, const DynamicArray *tnames,
                        DynamicArray *results) {
  if (!line)
    return 0;
  int count = 0;

  /* message: "literal", required_error: "literal" */
  for (int ki = 0; ZOD_KEYS[ki]; ki++) {
    const char *p = line;
    while ((p = strstr(p, ZOD_KEYS[ki])) != NULL) {
      p += strlen(ZOD_KEYS[ki]);
      while (*p && isspace((unsigned char)*p))
        p++;
      if (*p != '"' && *p != '\'')
        continue;

      size_t val_start_pos = (size_t)(p - line);
      char val[256];
      size_t vlen;
      if (!read_quoted_string(&p, val, sizeof(val), &vlen))
        continue;
      if (!looks_like_human_text(val, vlen))
        continue;
      if (is_inside_translator_call(line, val_start_pos + 1, tnames))
        continue;

      add_result(results, file_path, line_num, val_start_pos + 2, "zod", val);
      count++;
    }
  }

  /* .min(n, "literal")  /  .max(n, "literal")  etc. — message is 2nd arg */
  for (int mi = 0; ZOD_TWO_ARG_METHODS[mi]; mi++) {
    const char *p = line;
    while ((p = strstr(p, ZOD_TWO_ARG_METHODS[mi])) != NULL) {
      p += strlen(ZOD_TWO_ARG_METHODS[mi]);

      /* Skip the first argument; stop at the comma */
      int depth = 1;
      int found_comma = 0;
      while (*p && depth > 0) {
        if (*p == '(')
          depth++;
        else if (*p == ')') {
          depth--;
          if (depth == 0)
            break;
        } else if (*p == ',' && depth == 1) {
          found_comma = 1;
          p++;
          break;
        }
        p++;
      }
      if (!found_comma)
        continue;

      while (*p && isspace((unsigned char)*p))
        p++;
      if (*p != '"' && *p != '\'')
        continue;

      size_t val_start_pos = (size_t)(p - line);
      char val[256];
      size_t vlen;
      if (!read_quoted_string(&p, val, sizeof(val), &vlen))
        continue;
      if (!looks_like_human_text(val, vlen))
        continue;
      if (is_inside_translator_call(line, val_start_pos + 1, tnames))
        continue;

      add_result(results, file_path, line_num, val_start_pos + 2, "zod", val);
      count++;
    }
  }

  /* .email("literal")  /  .url("literal")  etc. — message is 1st arg */
  for (int mi = 0; ZOD_ONE_ARG_METHODS[mi]; mi++) {
    const char *p = line;
    while ((p = strstr(p, ZOD_ONE_ARG_METHODS[mi])) != NULL) {
      p += strlen(ZOD_ONE_ARG_METHODS[mi]);
      while (*p && isspace((unsigned char)*p))
        p++;
      if (*p != '"' && *p != '\'')
        continue;

      size_t val_start_pos = (size_t)(p - line);
      char val[256];
      size_t vlen;
      if (!read_quoted_string(&p, val, sizeof(val), &vlen))
        continue;
      if (!looks_like_human_text(val, vlen))
        continue;
      if (is_inside_translator_call(line, val_start_pos + 1, tnames))
        continue;

      add_result(results, file_path, line_num, val_start_pos + 2, "zod", val);
      count++;
    }
  }
  return count;
}

/* =====================================================================
   PATTERN 6 — {"literal"}  string literal in JSX expression block
   ===================================================================== */
int detect_jsx_expr_strings(const char *line, size_t line_num,
                            const char *file_path, const DynamicArray *tnames,
                            DynamicArray *results) {
  if (!line)
    return 0;
  size_t len = strlen(line);
  int count = 0;

  for (size_t i = 0; i < len; i++) {
    if (line[i] != '{')
      continue;

    /* Skip whitespace inside the brace */
    size_t j = i + 1;
    while (j < len && isspace((unsigned char)line[j]))
      j++;
    if (j >= len || (line[j] != '"' && line[j] != '\''))
      continue;

    size_t val_start_pos = j;
    const char *p = &line[j];
    char val[256];
    size_t vlen;
    if (!read_quoted_string(&p, val, sizeof(val), &vlen))
      continue;

    /* A closing '}' must follow (with optional whitespace) */
    while (*p && isspace((unsigned char)*p))
      p++;
    if (*p != '}')
      continue;

    if (!looks_like_human_text(val, vlen))
      continue;
    if (is_inside_translator_call(line, val_start_pos + 1, tnames))
      continue;

    add_result(results, file_path, line_num, val_start_pos + 2, "jsx-expr",
               val);
    count++;
    i = (size_t)(p - line);
  }
  return count;
}

/* =====================================================================
   PATTERN 7 — throw new Error("literal")
   ===================================================================== */
static const char *THROW_PATTERNS[] = {
    "throw new Error(", "throw new TypeError(", "throw new RangeError(", NULL};

int detect_throw_errors(const char *line, size_t line_num,
                        const char *file_path, DynamicArray *results) {
  if (!line)
    return 0;
  int count = 0;

  for (int pi = 0; THROW_PATTERNS[pi]; pi++) {
    const char *p = strstr(line, THROW_PATTERNS[pi]);
    if (!p)
      continue;
    p += strlen(THROW_PATTERNS[pi]);
    while (*p && isspace((unsigned char)*p))
      p++;
    if (*p != '"' && *p != '\'')
      continue;

    size_t val_start_pos = (size_t)(p - line);
    char val[256];
    size_t vlen;
    if (!read_quoted_string(&p, val, sizeof(val), &vlen))
      continue;
    if (!looks_like_human_text(val, vlen))
      continue;

    add_result(results, file_path, line_num, val_start_pos + 2, "throw", val);
    count++;
  }
  return count;
}

/* =====================================================================
   MAIN SCANNER — drives all detectors over a file line by line
   ===================================================================== */
int scan_file_for_untranslated(const char *file_path, const FileBuffer *buffer,
                               const ParserConfig *config,
                               DynamicArray *results) {
  if (!file_path || !buffer || !buffer->content || !config || !results) {
    fprintf(stderr, "Error: Invalid inputs to scan_file_for_untranslated\n");
    return -1;
  }

  /* Phase 1: collect all translator variable names declared in this file */
  DynamicArray *tnames = da_create();
  if (!tnames)
    return -1;
  collect_translator_names(buffer->content, buffer->size, tnames);

  /* Phase 2: process line by line */
  const char *content = buffer->content;
  size_t size = buffer->size;
  size_t line_num = 1;
  size_t i = 0;

  while (i < size) {
    size_t line_start = i;
    while (i < size && content[i] != '\n')
      i++;
    size_t line_end = i;
    if (i < size)
      i++; /* skip the newline */

    size_t line_len = line_end - line_start;
    if (line_len >= MAX_LINE_LENGTH) {
      line_num++;
      continue;
    }

    char line_buf[MAX_LINE_LENGTH];
    strncpy(line_buf, &content[line_start], line_len);
    line_buf[line_len] = '\0';

    if (!is_safe_line(line_buf)) {
      detect_jsx_text_nodes(line_buf, line_num, file_path, tnames, results);
      detect_string_props(line_buf, line_num, file_path, tnames, results);
      detect_toast_literals(line_buf, line_num, file_path, tnames, results);
      detect_set_state_literals(line_buf, line_num, file_path, tnames, results);
      detect_zod_messages(line_buf, line_num, file_path, tnames, results);
      detect_jsx_expr_strings(line_buf, line_num, file_path, tnames, results);
      detect_throw_errors(line_buf, line_num, file_path, results);
    }
    line_num++;
  }

  da_free(tnames);
  return 0;
}
