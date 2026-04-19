#include "jsonw.h"
#include <ctype.h>
#include <string.h>

#define OUT_PARAM(TYPE, IDENT)                                                 \
  TYPE _out_param_##IDENT;                                                     \
  if (IDENT == NULL)                                                           \
  IDENT = &_out_param_##IDENT

#define JSON_ESCAPES "\"\\/bfnrt"
#define JSON_CODEPTS "\"\\/\b\f\n\r\t"

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

char *jsonw_ws(char *json) {
  return json ? json + strspn(json, " \t\n\r") : json;
}

#define WSCHRWS(CHR, JSON) jsonw_ws(jsonw_litchr(CHR, jsonw_ws(JSON)))

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

char *jsonw_utf16esc(unsigned *cu, char *json) {
  OUT_PARAM(unsigned, cu);
  if (json == NULL)
    return NULL;

  if (*json++ != '\\' || *json++ != 'u')
    return NULL;

  unsigned codeut = 0; // only write to `cu` on success
  for (int i = 0; i < 4; i++) {
    if (!isxdigit(*json))
      return NULL;
    codeut <<= 4;
    codeut |= isdigit(*json) ? *json++ - '0' : tolower(*json++) - 'a' + 10;
  }

  return *cu = codeut, json;
}

char *jsonw_character(char *chr, char *json) {
  OUT_PARAM(char, chr);
  if (json == NULL)
    return NULL;

  // the parser state is a single pointer into a JSON text. \uXXXX escapes can
  // produce several UTF-8 code units so we need to keep track of which one is
  // to be output next. to do so we allow the parser state to point part way
  // through a \uXXXX escape; so if the parser state is three characters into
  // a \uXXXX escape then the third UTF-8 code unit for that escape is to be
  // output next. this code fragment backtracks to the beginning of a \uXXXX
  // escape if we're part way through one
  char *loc = json;
  while (loc - json < 4 && isxdigit(*json))
    json--;
  if (*json-- == 'u' && *json == '\\' || (json = loc))
    for (char *j = json; *j-- == '\\' && *j-- == '\\';)
      *j == '\\' || (json = loc);
  int i = loc - json;

  // when we encounter a \uXXXX escape, parse it into a code point then encode
  // it into UTF-8. I can't think of a way around it, because ultimately we want
  // string literals like "\xc4\xa3" and "\\u0123" to compare equal
  unsigned codeut;
  if (jsonw_utf16esc(&codeut, json)) {
    long codept = (json += 6, codeut);
    if (0xd800 <= codeut && codeut < 0xdc00 && jsonw_utf16esc(&codeut, json) &&
        0xdc00 <= codeut && codeut < 0xe000) // surrogate pair
      json += 6, codept = ((codept & 0x3ff) << 10 | codeut & 0x3ff) + 0x10000;

    // the RFC points out that the grammar allows string literals to contain
    // lone surrogates, but doesn't elaborate; namely it doesn't specify what
    // sequence of code points the string literal should stand for. according
    // to Unicode, invalid code points should be replaced by U+FFFD REPLACEMENT
    // CHARACTER, but if we do that then string literals which contain different
    // invalid Unicode in different places could compare equal. encoding lone
    // surrogates into invalid UTF-8 in the canonical way might just be the best
    // we can do
    int n = 3 - (codept < 0x10000) - (codept < 0x800) - (codept < 0x80);
    codept >>= 6 * (n - i);
    codept &= i ? 0x3f : 0xff;
    codept |= i ? 0x80 : (int[]){0x00, 0xc0, 0xe0, 0xf0}[n];
    return *chr = codept, i < n ? loc + 1 : json;
  }

  if (*json == '\\' && json++) {
    char *escape, *escapes = JSON_ESCAPES;
    if (*json && (escape = strchr(escapes, *json)) && json++)
      return *chr = JSON_CODEPTS[escape - escapes], json;
    return NULL;
  }

  if ((unsigned char)*json >= ' ' && *json != '"')
    return *chr = *json, ++json;

  return NULL;
}

char *jsonw_string(size_t *len, char *json) {
  OUT_PARAM(size_t, len);
  size_t length = 0; // only write to `len` on success
  char *j = json = jsonw_beginstr(json);
  while (j = jsonw_character(NULL, j))
    json = j, length++;
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
  size_t length = 0; // only write to `len` on success
  char *j = json = jsonw_beginarr(json);
  if (j = jsonw_endarr(json))
    return *len = length, j;
  for (; json = jsonw_value(NULL, json); json = jsonw_valuesep(json), length++)
    if (j = jsonw_endarr(json))
      return *len = length, j;
  return NULL;
}

char *jsonw_object(size_t *len, char *json) {
  OUT_PARAM(size_t, len);
  size_t length = 0; // only write to `len` on success
  char *j = json = jsonw_beginobj(json);
  if (j = jsonw_endobj(json))
    return *len = length, j;
  for (; json = jsonw_value(NULL, jsonw_name(json));
       json = jsonw_valuesep(json), length++)
    if (j = jsonw_endobj(json))
      return *len = length, j;
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
  // `jsonw_array` calls `jsonw_value` and `jsonw_value` calls `jsonw_array`,
  // resulting in a cycle when `json == NULL`. this check breaks that cycle
  if (json == NULL)
    return NULL;

  // no need to `OUT_PARAM(json_ty, type)` because we don't dereference it
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
      return json;
  while (json = jsonw_member(json));
  return NULL;
}

char *jsonw_lookup(char *name, char *json) {
  return jsonw_name(jsonw_find(name, json));
}

char *jsonw_quote(char buf[sizeof "\\u0000"], char chr) {
  char *codept, *codepts = JSON_CODEPTS;
  if (chr && (codept = strchr(codepts, chr)))
    return *buf++ = '\\', *buf++ = JSON_ESCAPES[codept - codepts], buf;

  if ((unsigned char)chr >= ' ' && chr != '"')
    return *buf++ = chr, buf;

  // only control codes less than ' ' remain
  strcpy(buf, "\\u00"), buf += 4;
  unsigned char lo = chr & 0xf, hi = chr >> 4 & 0xf;
  *buf++ = hi + '0', *buf++ = lo < 10 ? lo + '0' : lo - 10 + 'a';
  return buf;
}

char *jsonw_escape(char *buf, size_t size, char *str) {
  // the size of `buf` shall be strictly greater than zero
  for (char *end = buf + size; *str && end - buf >= sizeof "\\u0000"; str++)
    buf = jsonw_quote(buf, *str);
  return *buf = '\0', str;
}

char *jsonw_unescape(char *buf, size_t size, char *json) {
  // the size of `buf` shall be strictly greater than zero
  for (char *j = json; size-- > 1 && (j = jsonw_character(buf, j)); buf++)
    json = j;
  return *buf = '\0', json;
}
