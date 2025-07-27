#include "jsonw.h"
#include <stdio.h>

char *json = "[{ \"name\": \"John\", \"birth\": 1978 }, { \"birth\": 2010 }, "
             "{ \"name\": \"too long\" }, { \"birth\": 1.2 }, { \"age\": 5 }]";

struct person {
  char name[8];
  short birth;
};

char *deserialize_person(struct person *person, char **warn, char *json) {
  for (char *memb = jsonw_beginobj(json); memb; memb = jsonw_member(memb)) {
    if (jsonw_strcmp("name", jsonw_beginstr(memb)) == 0) {
      if (jsonw_endstr(jsonw_unescape(person->name, sizeof(person->name),
                                      jsonw_beginstr(jsonw_name(memb)))))
        continue;
      *warn = "invalid name";
    }

    if (jsonw_strcmp("birth", jsonw_beginstr(memb)) == 0) {
      double birth;
      if (jsonw_number(&birth, jsonw_name(memb)) &&
          (person->birth = birth) == birth)
        continue;
      *warn = "invalid birth";
    }

    if (*warn == NULL)
      *warn = "unknown key";
  }

  return jsonw_object(NULL, json);
}

int main(void) {
  for (char *elem = jsonw_beginarr(json); elem; elem = jsonw_element(elem)) {
    struct person person = {.name = "", .birth = 0};
    char *warn = NULL;
    if (deserialize_person(&person, &warn, elem)) {
      printf("(struct person){\"%s\", %d}", person.name, person.birth);
      printf(warn ? " (%s)\n" : "\n", warn);
    }
  }
}

/*
#include "jsonw.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  long size;
  char *buf = NULL;
  while (*++argv) {
    // printf("parsing %s\n", *argv);

    FILE *fp = fopen(*argv, "r");
    if (fp == NULL)
      perror("fopen"), exit(EXIT_FAILURE);
    if (fseek(fp, 0, SEEK_END) == -1)
      perror("fseek"), exit(EXIT_FAILURE);
    if ((size = ftell(fp)) == -1)
      perror("ftell"), exit(EXIT_FAILURE);
    rewind(fp);

    buf = realloc(buf, size + 1);
    if (fread(buf, 1, size, fp) != size)
      perror("fread"), exit(EXIT_FAILURE);
    buf[size] = '\0';
    if (fclose(fp) == EOF)
      perror("fclose"), exit(EXIT_FAILURE);

    char *end = jsonw_text(NULL, buf);
    bool valid = end && end - buf == size;
    if (**argv == 'y' && !valid || **argv == 'n' && valid)
      printf("test failed: %s:\n%s\n", *argv, buf);
    if (**argv != 'y' && **argv != 'n' && **argv != 'i')
      printf("invalid test: %s\n", *argv);
  }

  free(buf);
}
*/
