#ifndef GTMXC_TYPES_H
#define GTMXC_TYPES_H

typedef int  gtm_status_t;
typedef int  gtm_int_t;
typedef long gtm_long_t;
typedef char gtm_char_t;

typedef struct {
  gtm_long_t length;
  gtm_char_t *address;
} gtm_string_t;

void *gtm_malloc(size_t);
void gtm_free(void *);

#endif
