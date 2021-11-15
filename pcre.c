#define _GNU_SOURCE  /* memmem() */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "gtmxc_types.h"

#define ERROR_BASE 16384
#define MSTR_LIMIT 1048576
#define UNUSED __attribute__((unused))
#define EXPORT __attribute__((visibility("default")))

typedef struct {
  gtm_string_t;
} input_t;

typedef gtm_string_t output_t;

enum error_number {
  E_OK = 0,
  E_INTERNAL,
  E_SLASH,
  E_OPT,
  E_PATTERN,
  E_MATCH,
  E_SUBST,
  E_LIMIT,
  E_MEM,
  E_END,
  E_GROUP,
};

char *error_messages[] = {
  [E_OK]       = "",
  [E_INTERNAL] = "%PCRE-E-INTERNAL, Internal error",
  [E_SLASH]    = "%PCRE-E-SLASH, Missing slash in search pattern",
  [E_OPT]      = "%PCRE-E-OPT, Invalid options",
  [E_PATTERN]  = "%PCRE-E-PATTERN, Compilation failed",
  [E_MATCH]    = "%PCRE-E-MATCH, Match error: ",
  [E_SUBST]    = "%PCRE-E-SUBST, Substitution error: ",
  [E_LIMIT]    = "&PCRE-E-LIMIT, M string length limit exceeded",
  [E_MEM]      = "%PCRE-E-MEM, Out of memory",
  [E_END]      = "%PCRE-E-END, No more matches",
  [E_GROUP]    = "%PCRE-E-GROUP, Invalid capture group name or index",
};

typedef struct {
  const char *func;
  int number;
  struct {
    char text[300];
    int length;
  } append;
} error_t;

static error_t last_error;

static void clear_error(error_t *error, const char *func) {
  error->func = func;
  error->number = E_OK;
  error->append.length = 0;
}

#define ERROR(code) ({ \
    error->number = code; \
    -1; \
  })

#define ERROR_NULL(code) ({ \
    error->number = code; \
    NULL; \
  })

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define max(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

static output_t *copy_mem(error_t *error, char *address, int length) {
  output_t *output = gtm_malloc(sizeof(*output));
  if (!output) {
    return ERROR_NULL(E_MEM);
  }
  output->address = gtm_malloc(max(length, 1));  // malloc(0) is not well defined
  if (!output->address) {
    return ERROR_NULL(E_MEM);
  }
  memcpy(output->address, address, length);
  output->length = length;
  return output;
}

static output_t *copy(error_t *error, input_t *input) {
  return copy_mem(error, input->address, input->length);
}

static output_t *empty_string(error_t *error) {
  return copy_mem(error, "", 0);
}

static void reverse(char *begin, char *end) {
  char *p = begin;
  char *q = end - 1;
  while (p < q) {
    char c = *p;
    *p = *q;
    *q = c;
    p++;
    q--;
  }
}

static void put_int(char **begin, int n) {
  char *p = *begin;
  char *q = p;
  if (n < 0) {
    *p = '-';
    p++;
    q++;
    n *= -1;
  }
  do {
    *p = (n % 10) + '0';
    n /= 10;
    p++;
  } while (n > 0);
  reverse(q, p);
  *begin = p;
}

static output_t *int_string(error_t *error, int n) {
  char s[11];  // -2,147,483,648
  char *p = s;
  put_int(&p, n);
  return copy_mem(error, s, p - s);
}

static output_t *error_string(error_t *error) {
  char text[128 + sizeof(error->append.text)];
  input_t input = { .address = text };
  input.length = snprintf(text, sizeof(text), "%d,&pcre.%s,%s%.*s", ERROR_BASE + error->number, error->func, error_messages[error->number], error->append.length, error->append.text);
  return copy(error, &input);
}

EXPORT gtm_string_t *error(UNUSED int argc) {
  error_t *error = &last_error;
  if (error->number == E_OK) {
    return empty_string(error);
  }
  return error_string(error);
}

static void error_append(error_t *error, char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int remaining = sizeof(error->append.text) - error->append.length;
  error->append.length += vsnprintf(error->append.text + error->append.length, remaining, format, ap);
  va_end(ap);
}

static void error_append_pcre_message(error_t *error, int pcre_number) {
  int remaining = sizeof(error->append.text) - error->append.length;
  error->append.length += pcre2_get_error_message(pcre_number, (PCRE2_UCHAR8 *)error->append.text + error->append.length, remaining);
}

typedef struct {
  pcre2_code *re;
  uint32_t groups;
  pcre2_match_data *data;
  int all;
  int vector;
  input_t text;
  input_t sep;
  int utf8;
  int crlf;
  int next;
  struct {
    PCRE2_SPTR table;
    uint32_t entry_size;
    uint32_t count;
  } names;
} context_t;

static context_t match_context;

static void clear_context(context_t *context) {
  if (!context->re) {
    return;
  }
  pcre2_code_free(context->re);
  if (context->data) {
    pcre2_match_data_free(context->data);
  }
  if (context->text.address) {
    free(context->text.address);
  }
  if (context->sep.address) {
    free(context->sep.address);
  }
  memset(context, '\0', sizeof(*context));
}

static int copy_input(error_t *error, input_t *dst, input_t *src) {
  dst->address = malloc(src->length);
  if (!dst->address) {
    return ERROR(E_MEM);
  }
  memcpy(dst->address, src->address, src->length);
  dst->length = src->length;
  return 0;
}

typedef struct {
  int i;  // PCRE2_CASELESS
  int m;  // PCRE3_MULTILINE
  int s;  // PCRE2_DOTALL
  int x;  // PCRE2_EXTENDED 
  int g;  // global
  int z;  // no PCRE2_UTF|PCRE2_UCP, much faster, as always in M: 'z' is synonim for speed
  int a;  // all matched string in first record field
  int v;  // return ovector in record
} regex_opts_t;

static int parse_regex_opts(regex_opts_t *opts, char *begin, char *end) {
  memset(opts, '\0', sizeof(*opts));
  char *p = begin;
  while (p < end) {
    switch (*p) {
      case 'i':
        opts->i++;
        break;
      case 'm':
        opts->m++;
        break;
      case 's':
        opts->s++;
        break;
      case 'x':
        opts->x++;
        break;
      case 'g':
        opts->g++;
        break;
      case 'z':
        opts->z++;
        break;
      case 'a':
        opts->a++;
        break;
      case 'v':
        opts->v++;
        break;
      default:
        return -1;
    }
    p++;
  }
  return 0;
}

static uint32_t regex_compile_options(regex_opts_t *opts) {
  uint32_t options = 0;
  if (opts->i) {
    options |= PCRE2_CASELESS;
  }
  if (opts->m) {
    options |= PCRE2_MULTILINE;
  }
  if (opts->s) {
    options |= PCRE2_DOTALL;
  }
  if (opts->x) {
    options |= opts->x > 1 ? PCRE2_EXTENDED_MORE : PCRE2_EXTENDED;
  }
  if (!opts->z) {
    options |= PCRE2_UTF | PCRE2_UCP;
  }
  return options;
}

static int parse_regex(error_t *error, input_t *regex, input_t *pattern, regex_opts_t *opts) {
  if (pattern->length < 2 || pattern->address[0] != '/') {
    return ERROR(E_SLASH);
  }
  regex->address = pattern->address + 1;
  regex->length = pattern->length - 1;
  char *last_slash = memrchr(regex->address, '/', regex->length);
  if (!last_slash) {
    return ERROR(E_SLASH);
  }
  regex->length = last_slash - regex->address;
  if (parse_regex_opts(opts, last_slash + 1, pattern->address + pattern->length)) {
    return ERROR(E_OPT);
  }
  return 0;
}

static int regex_compile(error_t *error, pcre2_code **re, regex_opts_t *opts, input_t *pattern) {
  input_t regex;
  if (parse_regex(error, &regex, pattern, opts)) {
    return -1;
  }
  uint32_t compile_options = regex_compile_options(opts);
  int error_number;
  PCRE2_SIZE error_offset;
  *re = pcre2_compile((PCRE2_SPTR)regex.address, regex.length, compile_options, &error_number, &error_offset, NULL);
  if (!*re) {
    error_append(error, " at offset %d: ", (int)error_offset);
    error_append_pcre_message(error, error_number);
    return ERROR(E_PATTERN);
  }
  return 0;
}

EXPORT gtm_string_t *replace(int argc, input_t *text, input_t *search, input_t *replace) {
  error_t *error = &last_error;
  clear_error(error, __func__);
  clear_context(&match_context);
  if (argc < 1) {
    return empty_string(error);
  }
  if (argc < 2) {
    return copy(error, text);
  }
  regex_opts_t opts;
  pcre2_code *re;
  if (regex_compile(error, &re, &opts, search)) {
    return NULL;
  }
  if (argc < 3) {
    pcre2_code_free(re);              
    return copy(error, text);
  }
  int substitute_options = 0;
  if (opts.g) {
    substitute_options |= PCRE2_SUBSTITUTE_GLOBAL;
  }
  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);  // do it here or pcre2_substitute will do it twice
  PCRE2_SIZE length = 0;
  int rc = pcre2_substitute(re, (PCRE2_SPTR)text->address, text->length, 0, substitute_options | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH, match_data, NULL, (PCRE2_SPTR)replace->address, replace->length, NULL, &length);
  if (rc != PCRE2_ERROR_NOMEMORY) {
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);              
    error_append_pcre_message(error, rc);
    return ERROR_NULL(E_SUBST);
  }
  if (length > MSTR_LIMIT) {
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);              
    return ERROR_NULL(E_LIMIT);
  }
  output_t *output = gtm_malloc(sizeof(*output));
  output->address = gtm_malloc(length);
  if (!output->address) {
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);              
    return ERROR_NULL(E_MEM);
  }
  rc = pcre2_substitute(re, (PCRE2_SPTR)text->address, text->length, 0, substitute_options, match_data, NULL, (PCRE2_SPTR)replace->address, replace->length, (PCRE2_UCHAR8*)output->address, &length);
  output->length = length;
  pcre2_match_data_free(match_data);
  pcre2_code_free(re);              
  if (rc < 0) {
    gtm_free(output->address);
    gtm_free(output);
    error_append_pcre_message(error, rc);
    return ERROR_NULL(E_SUBST);
  }
  return output;
}

EXPORT gtm_string_t *test(int argc, input_t *text, input_t *search) {
  error_t *error = &last_error;
  clear_error(error, __func__);
  clear_context(&match_context);
  if (argc < 2) {
    return int_string(error, 0);
  }
  input_t null = { .address = "", .length = 0 };
  if (!text->address) {
    text = &null;  // pcre2_match() doesn't accept .address=NULL
  }
  regex_opts_t opts;
  pcre2_code *re;
  if (regex_compile(error, &re, &opts, search)) {
    return NULL;
  }
  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);
  int rc = pcre2_match(re, (PCRE2_SPTR)text->address, text->length, 0, 0, match_data, NULL);
  if (rc < 0) {
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);              
    if (rc == PCRE2_ERROR_NOMATCH) {
      return int_string(error, 0);
    }
    error_append_pcre_message(error, rc);
    return ERROR_NULL(E_MATCH);
  }
  if (!opts.g) {
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);              
    return int_string(error, 1);
  }
  PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
  uint32_t option_bits;
  pcre2_pattern_info(re, PCRE2_INFO_ALLOPTIONS, &option_bits);
  int utf8 = (option_bits & PCRE2_UTF) != 0;
  uint32_t newline;
  int crlf = 0;
  pcre2_pattern_info(re, PCRE2_INFO_NEWLINE, &newline);
  switch (newline) {
    case PCRE2_NEWLINE_ANY:
    case PCRE2_NEWLINE_CRLF:
    case PCRE2_NEWLINE_ANYCRLF:
      crlf = 1;
  }
  int count = 1;
  for (;;) {
    uint32_t match_options = 0;
    PCRE2_SIZE offset = ovector[1];
    if (ovector[0] == ovector[1]) {
      if ((int)ovector[0] == text->length) {
        break;
      }
      match_options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
    }
    int rc = pcre2_match(re, (PCRE2_SPTR)text->address, text->length, offset, match_options, match_data, NULL);
    if (rc == PCRE2_ERROR_NOMATCH) {
      if (!match_options) {
        break;
      }
      ovector[1] = offset + 1;
      if (crlf && (int)offset < text->length - 1 && text->address[offset] == '\r' && text->address[offset + 1] == '\n') {
        ovector[1]++;
      } else if (utf8) {
        while ((int)ovector[1] < text->length) {
          if ((text->address[ovector[1]] & 0xc0) != 0x80) {
            break;
          }
          ovector[1]++;
        }
      }
      continue;
    }
    if (rc < 0) {
      pcre2_match_data_free(match_data);
      pcre2_code_free(re);
      error_append_pcre_message(error, rc);
      return ERROR_NULL(E_MATCH);
    }
    count++;
  }
  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
  return int_string(error, count);
}

static void store_mem(char **begin, char *address, int length, int write) {
  char *p = *begin;
  if (write) {
    memcpy(p, address, length);
  }
  p += length;
  *begin = p;
}

static void store_int(char **begin, int n, int write) {
  char str[11];  // -2,147,483,648
  char *p = str;
  put_int(&p, n);
  store_mem(begin, str, p - str, write);
}

static void store_vec(char **begin, PCRE2_SIZE *ovector, int i, input_t *sep, int write) {
  char *p = *begin;
  store_int(&p, ovector[2*i] + 1, write);  // first M index
  store_mem(&p, sep->address, sep->length, write);
  store_int(&p, ovector[2*i+1], write);    // last M index
  *begin = p;
}

static void ovector2vec(output_t *output, PCRE2_SIZE *ovector, int i, input_t *sep) {
  int write = output->address != NULL;
  char *p = output->address;
  store_vec(&p, ovector, i, sep, write);
  output->length = p - output->address;
}

static void ovector2record(output_t *output, int groups, int matches, PCRE2_SIZE *ovector, input_t *text, input_t *sep, int all, int vector) {
  int write = output->address != NULL;
  char *p = output->address;
  int first = all ? 0 : 1;
  for (int i = first; i < groups + 1; i++) {
    if (i > first) {
      store_mem(&p, sep->address, sep->length, write);
    }
    if (vector) {
      if (i < matches && ovector[2*i] != PCRE2_UNSET) {
        store_vec(&p, ovector, i, sep, write);
      } else {
        store_mem(&p, sep->address, sep->length, write);
      }
    } else {
      if (i < matches) {
        char *address = text->address + ovector[2*i];
        int length = ovector[2*i+1] - ovector[2*i];
        store_mem(&p, address, length, write);
      }
    }
  }
  output->length = p - output->address;
}

static output_t *match_record(error_t *error, context_t *context) {
  int matches = (int)pcre2_get_ovector_count(context->data);
  PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(context->data);
  output_t *output = gtm_malloc(sizeof(*output));
  if (!output) {
    return ERROR_NULL(E_MEM);
  }
  output->address = NULL;
  ovector2record(output, context->groups, matches, ovector, &context->text, &context->sep, context->all, context->vector);
  output->address = gtm_malloc(max(output->length, 1));
  if (!output->address) {
    return ERROR_NULL(E_MEM);
  }
  ovector2record(output, context->groups, matches, ovector, &context->text, &context->sep, context->all, context->vector);
  return output;
}

EXPORT gtm_string_t *match(int argc, input_t *text, input_t *search, input_t *sep) {
  error_t *error = &last_error;
  clear_error(error, __func__);
  context_t *context = &match_context;
  clear_context(context);
  if (argc < 1) {
    return empty_string(error);
  }
  if (argc < 2) {
    return copy(error, text);
  }
  regex_opts_t opts;
  if (regex_compile(error, &context->re, &opts, search)) {
    return NULL;
  }
  pcre2_pattern_info(context->re, PCRE2_INFO_CAPTURECOUNT, &context->groups);
  input_t null = { .address = "", .length = 0 };
  if (!text->address) {
    text = &null;  // pcre2_match() doesn't accept .address=NULL
  }
  if (argc < 3) {
    sep = &null;
  }
  context->data = pcre2_match_data_create_from_pattern(context->re, NULL);
  int rc = pcre2_match(context->re, (PCRE2_SPTR)text->address, text->length, 0, 0, context->data, NULL);
  if (rc < 0) {
    clear_context(context);
    if (rc == PCRE2_ERROR_NOMATCH) {
      if (sep->length) {
        return empty_string(error);
      }
      return int_string(error, 0);
    }
    error_append_pcre_message(error, rc);
    return ERROR_NULL(E_MATCH);
  }
  if (opts.a) {
    context->all = 1;
  }
  if (opts.v) {
    context->vector = 1;
  }
  if (opts.g) {
    uint32_t options;
    pcre2_pattern_info(context->re, PCRE2_INFO_ALLOPTIONS, &options);
    if (options & PCRE2_UTF) {
      context->utf8 = 1;
    }
    uint32_t newline;
    pcre2_pattern_info(context->re, PCRE2_INFO_NEWLINE, &newline);
    switch (newline) {
      case PCRE2_NEWLINE_ANY:
      case PCRE2_NEWLINE_CRLF:
      case PCRE2_NEWLINE_ANYCRLF:
        context->crlf = 1;
    }
    context->next = 1;
  }
  if (copy_input(error, &context->text, text)) {
    clear_context(context);
    return NULL;
  }
  if (sep->length) {
    if (copy_input(error, &context->sep, sep)) {
      clear_context(context);
      return NULL;
    }
    return match_record(error, context);
  }
  return int_string(error, 1);
}

EXPORT gtm_int_t end(UNUSED int argc) {
  context_t *context = &match_context;
  return !context->next;
}

EXPORT gtm_string_t *next(UNUSED int argc) {
  error_t *error = &last_error;
  context_t *context = &match_context;
  if (!context->next) {
    return ERROR_NULL(E_END);
  }
  input_t *text = &context->text;
  input_t *sep = &context->sep;
  PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(context->data);
  for (;;) {
    uint32_t match_options = 0;
    PCRE2_SIZE offset = ovector[1];
    if (ovector[0] == ovector[1]) {
      if ((int)ovector[0] == text->length) {
        break;
      }
      match_options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
    }
    int rc = pcre2_match(context->re, (PCRE2_SPTR)text->address, text->length, offset, match_options, context->data, NULL);
    if (rc == PCRE2_ERROR_NOMATCH) {
      if (!match_options) {
        break;
      }
      ovector[1] = offset + 1;
      if (context->crlf && (int)offset < text->length - 1 && text->address[offset] == '\r' && text->address[offset + 1] == '\n') {
        ovector[1]++;
      } else if (context->utf8) {
        while ((int)ovector[1] < text->length) {
          if ((text->address[ovector[1]] & 0xc0) != 0x80) {
            break;
          }
          ovector[1]++;
        }
      }
      continue;
    }
    if (rc < 0) {
      clear_context(context);
      error_append_pcre_message(error, rc);
      return ERROR_NULL(E_MATCH);
    }
    if (sep->length) {
      return match_record(error, context);
    }
    return int_string(error, 1);
  }
  if (sep->length) {
    clear_context(context);
    return empty_string(error);
  }
  clear_context(context);
  return int_string(error, 0);
}

#define isdigit(n) \
  ({ __typeof__ (n) _n = (n); \
     _n >= '0' && _n <= '9'; })

static int parse_int(input_t *input, int *result, int max_digits) {
  if (input->length > max_digits) {
    return -1;
  }
  char *p = input->address;
  char *q = p + input->length;
  int n = 0;
  while (p < q && isdigit(*p)) {
    n *= 10;
    n += *p - '0';
    p++;
  }
  if (p < q) {
    return -1;
  }
  *result = n;
  return 0;
}

static int mem_eq(char *a, int a_length, char *b, int b_length) {
  if (a_length != b_length) {
    return -1;
  }
  return memcmp(a, b, a_length);
}

static int name2i(context_t *context, input_t *name, int *i) {
  if (!context->names.table) {
    pcre2_pattern_info(context->re, PCRE2_INFO_NAMECOUNT, &context->names.count);
    if (!context->names.count) {
      return -1;
    }
    pcre2_pattern_info(context->re, PCRE2_INFO_NAMETABLE, &context->names.table);
    pcre2_pattern_info(context->re, PCRE2_INFO_NAMEENTRYSIZE, &context->names.entry_size);
  }
  char *p = (char *)context->names.table;
  for (uint32_t j = 0; j < context->names.count; j++) {
    int n = (p[0] << 8) | p[1];
    char *address = p + 2;  // IMM2_SIZE
    int length = strlen(address);
    if (!mem_eq(address, length, name->address, name->length)) {
      *i = n;
      return 0;
    }
    p += context->names.entry_size;
  }
  return -1;
}

static output_t *vec_string(error_t *error, PCRE2_SIZE *ovector, int i, input_t *sep) {
  output_t *output = gtm_malloc(sizeof(*output));
  if (!output) {
    return ERROR_NULL(E_MEM);
  }
  output->address = NULL;
  ovector2vec(output, ovector, i, sep);
  output->address = gtm_malloc(max(output->length, 1));
  if (!output->address) {
    return ERROR_NULL(E_MEM);
  }
  ovector2vec(output, ovector, i, sep);
  return output;
}

typedef enum {
  GET_STRING,
  GET_ISSET,
  GET_ZVECTOR,
} get_mode_t;

static gtm_string_t *group_get(error_t *error, int argc, input_t *name, input_t *sep, get_mode_t mode) {
  context_t *context = &match_context;
  if (!context->re) {
    return ERROR_NULL(E_END);
  }
  if (argc < 1 || !name->length) {
    return ERROR_NULL(E_GROUP);
  }
  int i;
  if (parse_int(name, &i, 4)) {
    if (name2i(context, name, &i)) {
      return ERROR_NULL(E_GROUP);
    }
  } else {
    if (i >= (int)context->groups + 1) {
      return ERROR_NULL(E_GROUP);
    }
  }
  int matches = (int)pcre2_get_ovector_count(context->data);
  PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(context->data);
  switch (mode) {
    case GET_STRING:
      {
        if (i >= matches) {
          return empty_string(error);
        }
        input_t *text = &context->text;
        char *address = text->address + ovector[2*i];
        int length = ovector[2*i+1] - ovector[2*i];
        return copy_mem(error, address, length);
      }
    case GET_ISSET:
      {
        if (i < matches && ovector[2*i] != PCRE2_UNSET) {
          return int_string(error, 1);
        }
        return int_string(error, 0);
      }
    case GET_ZVECTOR:
      {
        if (i < matches && ovector[2*i] != PCRE2_UNSET) {
          return vec_string(error, ovector, i, sep);
        }
        return empty_string(error);
      }
  }
  return ERROR_NULL(E_INTERNAL);
}

EXPORT gtm_string_t *get(int argc, input_t *name) {
  error_t *error = &last_error;
  clear_error(error, __func__);
  return group_get(error, argc, name, NULL, GET_STRING);
}

EXPORT gtm_string_t *isset(int argc, input_t *name) {
  error_t *error = &last_error;
  clear_error(error, __func__);
  return group_get(error, argc, name, NULL, GET_ISSET);
}

EXPORT gtm_string_t *zvector(int argc, input_t *name, input_t *sep) {
  error_t *error = &last_error;
  clear_error(error, __func__);
  input_t pipe = { .address = "|", .length = 1 };
  if (argc < 2 || !sep->length) {
    sep = &pipe;
  }
  return group_get(error, argc, name, sep, GET_ZVECTOR);
}
