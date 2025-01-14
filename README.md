# JSONW

_Tiny RFC 8259-compliant JSON parser for C_

## Overview

JSONW is a library for incrementally walking JSON texts and extracting values from them. No parsing pass, no intermediate representation, no convenient high-level API; you’re in the driver’s seat, and it’s a stick shift. JSONW’s features include:

- ~200 lines of code;
- No dependencies;
- No dynamic allocation;
- Compliant with RFC 8259.

## Design

What JSONW provides is a set of functions we’ll call _parsers_; for a full list see [jsonw.h](jsonw.h). Parsers take a pointer into a JSON string and return a pointer one past the end of the syntactic construct they parsed, with some parsers producing metadata through out parameters. For example, the parser `jsonw_number` takes a pointer to the start of a JSON number and returns a pointer one past the end of that number, producing as metadata a `double` containing the number. And the parser `jsonw_array` skips over a JSON array and produces as metadata the number of elements in the array. To ignore the metadata, pass in `NULL`. All parsers behave like this.

Parsers are written as if wrapped by the `Maybe` monad: they return `NULL` on parse failure and short-circuit out when they receive `NULL` as input. The upshot is that parse errors generally take care of themselves: notice how the [examples](#examples) below have little to no error propagation logic yet handle malformed or misshaped JSON gracefully. In addition, out parameters are left untouched when a parser fails; see the [example on fallback values](#fallback%20values).

## Notes and Limits

Implementation notes:

- Objects with duplicate keys can be handled.
- `"\u0000"`s in string literals don’t cause issues.
- Unescaped `\x7F`s in string literals are supported.
- Lone surrogates like `"\uD800"` are parsed correctly.
- String literals are compared without normalization.

Implementation limits:

- Invalid UTF-8 in string literals is left untouched.
- The range and precision of numbers is that of C `double`s.
- Exponents are parsed as `unsigned short`s and can wrap around.
- Excessive nesting of structured types may overflow the stack.

Sanity-check RFC 8259 compliance with:

```sh
make test
sh -c 'cd tests/ && ../bin/test *.json' # should have no output
```

The tests in [tests/](tests/) are those found in Nicolas Seriot’s JSONTestSuite.

## Examples

### Printing Slices

```c
#include "jsonw.h"
#include <stdio.h>

char *json = "[\"foo\", { \"bar\": 123 }, [true], baz]";

void dump_value(char *json) {
  char *end = jsonw_value(NULL, json);
  printf(end ? "%.*s" : "(parse error)", (int)(end - json), json);
}

int main(void) {
  for (char *elem = jsonw_beginarr(json); elem; elem = jsonw_element(elem))
    dump_value(elem), putchar('\n');
}
```

```
"foo"
{ "bar": 123 }
[true]
(parse error)
```

### Type Metadata

```c
#include "jsonw.h"
#include <stdio.h>

char *json = "{ \"null\": null, \"true\": true, \"\\\"str\\\"\": \"str\"}";
char *types[] = {"NULL", "BOOLEAN", "NUMBER", "STRING", "ARRAY", "OBJECT"};

int main(void) {
  for (char *memb = jsonw_beginobj(json); memb; memb = jsonw_member(memb)) {
    char key[64];
    jsonw_ty type;
    if (jsonw_unescape(key, sizeof(key), jsonw_beginstr(memb)) &&
        jsonw_value(&type, jsonw_name(memb)))
      printf("key '%s' has value of type '%s'\n", key, types[type]);
  }
}
```

```
key 'null' has value of type 'NULL'
key 'true' has value of type 'BOOLEAN'
key '"str"' has value of type 'STRING'
```

### Duplicate Keys in Objects

```c
#include "jsonw.h"
#include <stdio.h>

char *json = "{ \"a\": 0.1, \"b\": 0.2, \"a\": 0.3 }";

int main(void) {
  for (char *elem = jsonw_beginobj(json); elem = jsonw_lookup("a", elem);
       elem = jsonw_element(elem)) {
    double value;
    if (jsonw_number(&value, elem))
      printf("%g\n", value);
  }
}
```

```
0.1
0.3
```

### `\u0000` in String Literals

```c
#include "jsonw.h"
#include <stdio.h>

char *json = "[\"abc\", \"\\u0000\", \"ab\\u0000c\"]";

int main(void) {
  for (char *elem = jsonw_beginarr(json); elem; elem = jsonw_element(elem)) {
    char c, *chars = jsonw_beginstr(elem);
    while (chars = jsonw_character(&c, chars))
      printf(c ? "%c" : "(NUL)", c);
    putchar('\n');
  }
}
```

```
abc
(NUL)
ab(NUL)c
```

### Fallback Values

```c
#include "jsonw.h"
#include <math.h>
#include <stdio.h>

void print_size(char *json) {
  double width = NAN, height = NAN, depth = 1; // fallback values
  char *size = jsonw_lookup("size", jsonw_beginobj(json));
  jsonw_number(&width, jsonw_lookup("width", jsonw_beginobj(size)));
  jsonw_number(&height, jsonw_lookup("height", jsonw_beginobj(size)));
  jsonw_number(&depth, jsonw_lookup("depth", jsonw_beginobj(size)));

  printf("%g x %g x %g\n", width, height, depth);
}

int main(void) {
  print_size("{ \"size\": { \"width\": 800, \"height\": 600, \"depth\": 4 } }");
  print_size("{ \"size\": { \"width\": \"800\", \"height\": 600 } }");
  print_size("{ \"size\": [] }");
  print_size("[\"foo\", \"bar\"]");
  print_size("42");
}
```

```
800 x 600 x 4
nan x 600 x 1
nan x nan x 1
nan x nan x 1
nan x nan x 1
```

### JSON Validation

```c
#include "jsonw.h"
#include <stdio.h>

char *types[] = {"NULL", "BOOLEAN", "NUMBER", "STRING", "ARRAY", "OBJECT"};

void validate_text(char *json) {
  jsonw_ty type;
  char *end = jsonw_text(&type, json);
  if (end && *end == '\0') // successful parse criteria
    printf("valid JSON text of type %s: %s\n", types[type], json);
  else
    printf("invalid JSON text: %s\n", json);
}

int main(void) {
  validate_text("null");
  validate_text(" 123\t");
  validate_text("[1, 2, 3]");
  validate_text("true false");
  validate_text("[1, 2, 3,]");
  validate_text("{ num: 123 }");
}
```

```
valid JSON text of type NULL: null
valid JSON text of type NUMBER:  123
valid JSON text of type ARRAY: [1, 2, 3]
invalid JSON text: true false
invalid JSON text: [1, 2, 3,]
invalid JSON text: { num: 123 }
```
