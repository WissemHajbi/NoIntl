# next-intl Patterns Reference
> For C program: detect static text NOT wrapped in t()

---

## IMPORTS (safe — skip these lines)

```
/^\s*import\s.*from\s+["']next-intl["']/
/^\s*import\s+\{[^}]*useTranslations[^}]*\}\s+from\s+["']next-intl["']/
/^\s*import\s+\{[^}]*useLocale[^}]*\}\s+from\s+["']next-intl["']/
/^\s*import\s+\{[^}]*useFormatter[^}]*\}\s+from\s+["']next-intl["']/
```

---

## TRANSLATION INIT (safe — skip these lines)

```
/const\s+\w+\s*=\s*useTranslations\s*\(\s*["'][^"']+["']\s*\)/
/const\s+\w+\s*=\s*getTranslations\s*\(\s*["'][^"']+["']\s*\)/
/const\s+locale\s*=\s*useLocale\s*\(\s*\)/
```

---

## TRANSLATED CALL PATTERNS (safe — these ARE translated)

| Pattern | Regex | Example |
|---|---|---|
| Simple key | `/\bt\s*\(\s*["'][^"']+["']\s*\)/` | `t("title")` |
| With vars | `/\bt\s*\(\s*["'][^"']+["']\s*,\s*\{[^}]*\}\s*\)/` | `t("key", { n: val })` |
| Dynamic key | `/\bt\s*\(\s*\w+[^)]*\)/` | `t(key.toLowerCase())` |
| Named translator | `/\b\w+\s*\(\s*["'][^"']+["']\s*(?:,\s*\{[^}]*\})?\s*\)/` | `errors("x.y")` |

> Any identifier assigned via `useTranslations()` is a translator. Treat `NAME(...)` as safe if NAME was assigned by `useTranslations`.

---

## STATIC TEXT PATTERNS TO FLAG

### JSX Text Nodes
```
# Literal text between tags (not inside {})
/>\s*[A-Za-z][A-Za-z0-9 ,.!?'"\-]{2,}\s*</
```
*Exclude: tag names, className values, self-closing tags*

### String Props (not t())
```
# placeholder="..." not using t()
/placeholder\s*=\s*["'][A-Za-z][^"']{2,}["']/

# title="..."
/title\s*=\s*["'][A-Za-z][^"']{2,}["']/

# aria-label="..."
/aria-label\s*=\s*["'][A-Za-z][^"']{2,}["']/

# alt="..."
/alt\s*=\s*["'][A-Za-z][^"']{2,}["']/

# label="..."
/label\s*=\s*["'][A-Za-z][^"']{2,}["']/

# description="..."
/description\s*=\s*["'][A-Za-z][^"']{2,}["']/
```

### Toast Calls with Hardcoded Strings
```
# toast.success("literal")
/toast\.(success|error|warning|info|message)\s*\(\s*["'][A-Za-z][^"']{1,}["']/

# toast with object message
/toast\.(success|error|warning|info|message)\s*\(\s*\{[^}]*message\s*:\s*["'][A-Za-z][^"']+["']/
```

### setError / setState with Hardcoded Strings
```
/set(?:Error|Warning|Message|State)\s*\(\s*["'][A-Za-z][^"']{2,}["']/
```

### Zod Schema Messages (hardcoded)
```
# message: "literal" — NOT using t()
/message\s*:\s*["'][A-Za-z][^"']{2,}["']/

# required_error: "literal"
/required_error\s*:\s*["'][A-Za-z][^"']{2,}["']/

# .min(n, "literal") / .max(n, "literal")
/\.(min|max|email|url|regex|refine)\s*\([^,)]*,\s*["'][A-Za-z][^"']{2,}["']/
```

### JSX Expression Blocks with Literal Strings
```
# {" some text "} or {'text'}
/\{\s*["'][A-Za-z][^"']{2,}["']\s*\}/
```

### Console / throw with Literal (optional — usually dev-only)
```
/console\.(log|error|warn)\s*\(\s*["'][A-Za-z][^"']{2,}["']/
/throw\s+new\s+Error\s*\(\s*["'][A-Za-z][^"']{2,}["']/
```

---

## SAFE PATTERNS — DO NOT FLAG

```
# Translation calls — SAFE
t("any.key")
t("any.key", { var: val })
errors("x.y")
tValidations("x.y")
ANY_TRANSLATOR("key")  // where ANY_TRANSLATOR = useTranslations(...)

# Technical strings — SAFE (ignore these)
/className\s*=\s*["'][^"']*["']/          // Tailwind classes
/type\s*=\s*["'](text|email|password|number|submit|button|checkbox|radio|file|hidden|date|tel|url)["']/
/variant\s*=\s*["'][^"']*["']/            // UI variants
/href\s*=\s*["'][^"']*["']/               // URLs
/src\s*=\s*["'][^"']*["']/                // Image src
/key\s*=\s*["'][^"']*["']/                // React keys
/id\s*=\s*["'][^"']*["']/                 // Element IDs
/name\s*=\s*["'][^"']*["']/               // Field names
/^\s*\/\//                                 // Comments
/^\s*\*\s/                                // JSDoc comments
/"use client"/                             // Directives
/"use server"/
```

---

## FILE SCOPE RULES

- **File extensions to scan**: `.tsx`, `.ts`, `.jsx`, `.js`
- **Skip directories**: `node_modules/`, `.next/`, `dist/`, `build/`
- **Skip files**: `*.config.*`, `*.test.*`, `*.spec.*`, `messages/**`

---

## ALGORITHM (C program logic)

```
for each file:
  collect all translator names:
    regex: /const\s+(\w+)\s*=\s*use(?:Translations|Formatter)\(/
    → names[] = captured group 1

  for each line:
    skip if matches IMPORT or COMMENT patterns
    skip if matches SAFE patterns (className, type, href, src, key, id, name, variant)
    
    for each string literal in line ["'...'"] or ['...']:
      if string length < 3: skip
      if string is all-lowercase no-spaces (likely a key): skip
      if string contains spaces or capital letters (likely human text):
        check if string is inside NAME(...) where NAME in names[]:
          → SAFE
        else:
          → FLAG as potential untranslated text
```

---

## ACTUAL CODEBASE PATTERNS OBSERVED

```tsx
// 1. Basic client component
const t = useTranslations("modules.auth.signIn");
{t("title")}

// 2. Multiple namespaces
const t = useTranslations("modules.discounts");
const errors = useTranslations("modules.discounts.errors");

// 3. Interpolation
{t("progress.stepOf", { current: currentStep, total: totalSteps })}

// 4. Toast (translated — SAFE)
toast.success(t("actions.success"));
toast.error(errors("general.serverError"));

// 5. Zod with t (translated — SAFE)
z.string().min(1, { message: t("name.required") })

// 6. Zod fallback (STATIC — FLAG)
z.string().min(1, { message: "Full name is required." })

// 7. Props (translated — SAFE)
<Input placeholder={t("fields.email.placeholder")} />

// 8. Props (STATIC — FLAG)
<Input placeholder="Enter your email" />

// 9. JSX text (STATIC — FLAG)
<h2>Welcome back</h2>

// 10. Dynamic key
t(lan.name.toLowerCase())

// 11. setError with literal (STATIC — FLAG)
setError("Something went wrong");

// 12. setError with t (SAFE)
setWarning(errors("password.incorrect"));
```
