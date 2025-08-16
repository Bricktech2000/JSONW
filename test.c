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
