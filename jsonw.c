#include "jsonw.h"
#include <ctype.h>
#include <string.h>

#define OUT_PARAM(TYPE, IDENT)                                                 \
  TYPE _out_param_##IDENT;                                                     \
  if (IDENT == NULL)                                                           \
  IDENT = &_out_param_##IDENT

// basic parsers

char *jsonw_litchr(char chr, char *json) {
  if (json && *json == chr)
    return ++json;
  return NULL;
}

char *jsonw_litstr(char *str, char *json) {
  if (json && strncmp(json, str, strlen(str)) == 0)
    return json + strlen(str);
  return NULL;
}

// characters

#define WSCHRWS(CHR, JSON) jsonw_ws(jsonw_litchr(CHR, jsonw_ws(JSON)))

char *jsonw_ws(char *json) {
  while (json && *json && strchr(" \t\n\r", *json))
    json++;
  return json;
}

char *jsonw_beginarr(char *json) { return WSCHRWS('[', json); }
char *jsonw_endarr(char *json) { return WSCHRWS(']', json); }
char *jsonw_beginobj(char *json) { return WSCHRWS('{', json); }
char *jsonw_endobj(char *json) { return WSCHRWS('}', json); }
char *jsonw_namesep(char *json) { return WSCHRWS(':', json); }
char *jsonw_valuesep(char *json) { return WSCHRWS(',', json); }
char *jsonw_beginstr(char *json) { return jsonw_litchr('"', json); }
char *jsonw_endstr(char *json) { return jsonw_litchr('"', json); }

#undef WSCHRWS

// values, texts

char *jsonw_null(char *json) { return jsonw_litstr("null", json); }

char *jsonw_boolean(bool *b, char *json) {
  OUT_PARAM(bool, b);
  char *j;
  (j = jsonw_litstr("true", json)) && (*b = true) ||
      (j = jsonw_litstr("false", json)) && (*b = false, 1);
  return j;
}

char *jsonw_number(double *num, char *json) {
  OUT_PARAM(double, num);
  if (json == NULL)
    return NULL;

  int sign = *json == '-' && json++ ? -1 : 1;
  double significand = 0.0;
  // using an unsigned type because integer overflow has undefined behavior
  unsigned short shift = 0, exponent = 0;

#define DIGIT_PLUS(LVALUE)                                                     \
  do {                                                                         \
    if (!isdigit(*json))                                                       \
      return NULL;                                                             \
    for (; isdigit(*json); json++)                                             \
      LVALUE *= 10, LVALUE += *json - '0';                                     \
  } while (0)

  if (*json == '0' && json++)
    ;
  else
    DIGIT_PLUS(significand);

  if (*json == '.' && json++) {
    char *j = json;
    DIGIT_PLUS(significand);
    shift = json - j;
  }

  if ((*json == 'e' || *json == 'E') && json++) {
    int sign = *json == '-' && json++ ? -1 : (*json == '+' && json++, 1);
    DIGIT_PLUS(exponent);
    exponent *= sign;
  }

#undef DIGIT_PLUS

  exponent -= shift, significand *= sign;
  // conversion from `unsigned short` to `short` preserves bit pattern
  for (; (short)exponent > 0; exponent--)
    significand *= 10;
  for (; (short)exponent < 0; exponent++)
    significand /= 10;

  return *num = significand, json;
}

char *jsonw_character(char *chr, char *json) {
  OUT_PARAM(char, chr);
  if (json == NULL)
    return NULL;

  if (*json == '\\' && ++json) {
    switch (*json++) {
    case '"':
    case '\\':
    case '/':
      return *chr = json[-1], json;
    case 'b':
      return *chr = '\b', json;
    case 'f':
      return *chr = '\f', json;
    case 'n':
      return *chr = '\n', json;
    case 'r':
      return *chr = '\r', json;
    case 't':
      return *chr = '\t', json;
    case 'u':;
      // ISO/IEC 9899:TC3, $5.2.4.2.1: `USHRT_MAX` is at least 65535
      unsigned short codepoint = 0;
      for (int i = 0; i < 4; i++) {
        codepoint <<= 4;
        if (isdigit(*json))
          codepoint |= *json - '0';
        else if (isxdigit(*json))
          codepoint |= tolower(*json) - 'a' + 10;
        else
          return NULL;
        json++;
      }

      // if a '\uXXXX' escape is beyond 7-bit ASCII, return a sentinel '\0'
      // and let clients handle things themselves if they so wish. unicode
      // shenanigans are beyond the scope of this library
      return *chr = codepoint <= 0x7f ? codepoint : '\0', json;
    }
    return NULL;
  }

  if ((unsigned char)*json >= ' ' && *json != '"')
    return *chr = *json, ++json;

  return NULL;
}

char *jsonw_string(size_t *len, char *json) {
  OUT_PARAM(size_t, len);
  size_t length; // only write to `len` on success
  char *j = json = jsonw_beginstr(json);
  for (length = 0; j = jsonw_character(NULL, j); json = j)
    length++;
  if (json = jsonw_endstr(json))
    return *len = length, json;
  return NULL;
}

char *jsonw_name(char *json) { return jsonw_namesep(jsonw_string(NULL, json)); }

char *jsonw_element(char *json) {
  return jsonw_valuesep(jsonw_value(NULL, json));
}

char *jsonw_member(char *json) { return jsonw_element(jsonw_name(json)); }

char *jsonw_array(size_t *len, char *json) {
  OUT_PARAM(size_t, len);
  size_t length; // only write to `len` on success
  char *j = json = jsonw_beginarr(json);
  if (j = jsonw_endarr(json))
    return j;
  for (length = 0; json = jsonw_value(NULL, json);
       json = jsonw_valuesep(json), length++)
    if (j = jsonw_endarr(json))
      return *len = length, j;
  return NULL;
}

char *jsonw_object(size_t *len, char *json) {
  OUT_PARAM(size_t, len);
  size_t length; // only write to `len` on success
  char *j = json = jsonw_beginobj(json);
  if (j = jsonw_endobj(json))
    return j;
  for (length = 0; json = jsonw_value(NULL, jsonw_name(json));
       json = jsonw_valuesep(json), length++)
    if (j = jsonw_endobj(json))
      return j;
  return NULL;
}

char *jsonw_primitive(jsonw_ty *type, char *json) {
  OUT_PARAM(jsonw_ty, type);
  char *j;
  (j = jsonw_null(json)) && (*type = JSONW_NULL, 1) ||
      (j = jsonw_boolean(NULL, json)) && (*type = JSONW_BOOLEAN) ||
      (j = jsonw_number(NULL, json)) && (*type = JSONW_NUMBER) ||
      (j = jsonw_string(NULL, json)) && (*type = JSONW_STRING);
  return j;
}

char *jsonw_structured(jsonw_ty *type, char *json) {
  OUT_PARAM(jsonw_ty, type);
  char *j;
  (j = jsonw_array(NULL, json)) && (*type = JSONW_ARRAY) ||
      (j = jsonw_object(NULL, json)) && (*type = JSONW_OBJECT);
  return j;
}

char *jsonw_value(jsonw_ty *type, char *json) {
  // no need to `OUT_PARAM(json_ty, type)` because we don't dereference it
  if (json == NULL)
    return NULL;

  char *j;
  (j = jsonw_primitive(type, json)) || (j = jsonw_structured(type, json));
  return j;
}

char *jsonw_text(jsonw_ty *type, char *json) {
  // no need to `OUT_PARAM(json_ty, type)` because we don't dereference it
  return jsonw_ws(jsonw_value(type, jsonw_ws(json)));
}

// utilities

int jsonw_strcmp(char *str, char *json) {
  // compare is performed without normalization
  char c;
  do
    c = '\0', json = jsonw_character(&c, json);
  while (*str && *str == c && str++);
  return *str - c;
}

char *jsonw_index(size_t idx, char *json) {
  while (idx--)
    json = jsonw_element(json);
  return json;
}

char *jsonw_find(char *name, char *json) {
  do
    if (jsonw_strcmp(name, jsonw_beginstr(json)) == 0)
      break;
  while ((json = jsonw_member(json)));
  return json;
}

char *jsonw_lookup(char *name, char *json) {
  return jsonw_name(jsonw_find(name, json));
}

char *jsonw_unescape(char *buf, size_t len, char *json) {
  // `len` shall be strictly greater than zero
  for (char *j = json; --len && (j = jsonw_character(buf, j)); buf++)
    json = j;
  return *buf = '\0', json;
}
