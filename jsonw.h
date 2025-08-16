#include <stdbool.h>
#include <stddef.h>

typedef enum {
  JSONW_NULL,
  JSONW_BOOLEAN,
  JSONW_NUMBER,
  JSONW_STRING,
  JSONW_ARRAY,
  JSONW_OBJECT,
} jsonw_ty;

// basic parsers
char *jsonw_litchr(char chr, char *json);  // literal character `chr`
char *jsonw_litstr(char *str, char *json); // literal string `str`

// characters
char *jsonw_ws(char *json);       // /[ \t\n\r]*/
char *jsonw_beginarr(char *json); // ws '[' ws
char *jsonw_endarr(char *json);   // ws ']' ws
char *jsonw_beginobj(char *json); // ws '{' ws
char *jsonw_endobj(char *json);   // ws '}' ws
char *jsonw_namesep(char *json);  // ws ':' ws
char *jsonw_valuesep(char *json); // ws ',' ws
char *jsonw_beginstr(char *json); // '"'
char *jsonw_endstr(char *json);   // '"'

// values, texts
char *jsonw_null(char *json);                       // 'null'
char *jsonw_boolean(bool *b, char *json);           // 'true' | 'false'
char *jsonw_number(double *num, char *json);        // e.g. '64', '-1.5e+10'
char *jsonw_character(char *chr, char *json);       // e.g. 'c', '\n', '\u007c'
char *jsonw_string(size_t *len, char *json);        // e.g. '"abc"', '"\tdef\n"'
char *jsonw_name(char *json);                       // e.g. '"abc":'
char *jsonw_element(char *json);                    // e.g. '"def",', '123,'
char *jsonw_member(char *json);                     // e.g. '"abc": 123,'
char *jsonw_array(size_t *len, char *json);         // e.g. '[ "def", 123 }'
char *jsonw_object(size_t *len, char *json);        // e.g. '{ "abc": 123 }'
char *jsonw_primitive(jsonw_ty *type, char *json);  // null | bool | num | str
char *jsonw_structured(jsonw_ty *type, char *json); // object | array
char *jsonw_value(jsonw_ty *type, char *json);      // primitive | structured
char *jsonw_text(jsonw_ty *type, char *json);       // ws value ws

// utilities
int jsonw_strcmp(char *str, char *json); // compare C string to JSON string lit
char *jsonw_index(size_t idx, char *json);  // get subscript `idx` of array
char *jsonw_find(char *name, char *json);   // find key `name` in object
char *jsonw_lookup(char *name, char *json); // look up value of key `name`
char *jsonw_quote(char buf[7], char chr);   // escape character lit
char *jsonw_escape(char *buf, size_t size, char *str);    // escape string lit
char *jsonw_unescape(char *buf, size_t size, char *json); // unescape string lit
