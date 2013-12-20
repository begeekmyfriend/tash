#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#define E2BIG  7
#define ENOEXEC  8
#define ENOENT  2

#define STDOUT  1

#define STRSIZ  512

char ab[STRSIZ];
char *ava[50];
char **av;
char *string;
int errno;
int ncoll;

void toolong()
{
  write(STDOUT, "Arg list too long\n", 18);
  exit(-1);
}

char *cat(char *as1, char *as2)
{
  char *s1, *s2;
  char c;

  s2 = string;
  s1 = as1;
  while ((c = *s1++) != '\0') {
    if (s2 > ab + STRSIZ)
      toolong();
    c &= 0x7f;
    if (c == '\0') {
      *s2++ = '/';  // Recover '/'
      break;
    }
    *s2++ = c;
  }

  s1 = as2;
  do {
    if (s2 > ab + STRSIZ)
      toolong();
    *s2++ = c = *s1++;
  } while (c != '\0');

  s1 = string;
  string = s2;
  return s1;
}

int compar(const char *s1, const char *s2)
{
  int ret;
  while ((ret = *s1 - *s2++) == 0 && *s1++ != '\0')
    continue;
  return ret;
}

// Bubble-sort
void sort(char **oav)
{
  char **p1, **p2;
  char **xchg;
  char *tmp;

  p2 = av;
  do {
    xchg = NULL;
    for (p1 = oav + 1; p1 < p2; p1++) {
      if (compar(*(p1 - 1), *p1) > 0) {
        xchg = p1;
        tmp = *p1;
        *p1 = *(p1 - 1);
        *(p1 - 1) = tmp;
      }
    }
    p2 = xchg;
  }  while (xchg != NULL);
}

void execute(char *file, char **args)
{
  execv(file, args);
  if (errno == ENOEXEC) {
    args[0] = file;
    *--args = "/bin/sh";  // ava[0]
    execv(*args, args);  // re-exec /bin/sh
  }
  if (errno == E2BIG)
    toolong();
}

int match1(char *str, char *pat)
{
  char s, p, rp, lp;
  int ok;
  int match2(char *, char *);

  if ((s = *str++) != '\0')
    if (!(s & 0x7f))
      s = 0x80;

  switch (p = *pat++) {

    case '[':
      lp = 0xff;
      while ((rp = *pat++) != '\0') {
        if (rp == ']') {
          if (ok)
            return match1(str, pat);
          else
            return 0;
        } else if (rp == '-') {
          if (s >= lp && s <= *pat++)
            ok++;
        } else {
          if (s == (lp = rp))
            ok++;  // at least match one char will be ok
        }
      }
      return 0;  // missing ']'

    default:
      if (s != p)  // not match
        return 0;

    case '?':
      if (s != '\0')
        return match1(str, pat);
      return 0;  // Yet pattern not finished

    case '*':
      return match2(--str, pat);

    case '\0':
      return (s == '\0');  // both finished
  }

  return 0;
}

int match2(char* str, char *pat)
{
  if (*pat == '\0')  // '*' is the last char
    return 1;
  while (*str != '\0') {
    if (match1(str++, pat))  // '*' match finished when pattern matched
      return 1;
  }
  return 0;  // Yet remain pattern char not matched
}

int match(char* str, char *pat)
{
  int match1(char *, char *);

  // Omit hidden files
  if (*str == '.' && *pat != '.')
    return 0;
  return match1(str, pat);
}

void expand(char *as)
{
  char *s, *cs;
  DIR *dir;
  char **oav;
  struct dirent *direp;

  cs = as;
  s = cs;
  while (*cs != '*' && *cs != '?' && *cs != '[') {
    if (*cs++ == '\0') {
      *av++ = cat(s, "");
      return;
    }
  }
  
  for (;;) {
    // There's no '/' character
    if (cs == s) {
      dir = opendir(".");
      s = "";
      break;
    }

    if (*--cs == '/') {
      *cs = '\0';
      dir = opendir(s == cs ? "/" : s);
      *cs++ = 0x80;  // Indicator
      break;
    }
  }

  if (dir < 0) {
    write(STDOUT, "No directory\n", 13);
    exit(-1);
  }

  oav = av;
  while ((direp = readdir(dir)) != NULL) {
    if (match(direp->d_name, cs)) {
      *av++ = cat(s, direp->d_name);
      ncoll++;
    }
  }
  closedir(dir);
  sort(oav);
}

int main(int argc, char **argv)
{
  char *cp;

  string = ab;
  av = &ava[1];  // ava[0] is for "/bin/sh"

  if (argc < 3) {
    write(STDOUT, "Arg count\n", 10);
    return 0;
  }

  argv++;
  *av++ = *argv;
  while (--argc >= 2)
    expand(*++argv);

  if (ncoll == 0) {
    write(STDOUT, "No match\n", 9);
    return 0;
  }

  execute(ava[1], &ava[1]);
  cp = cat("/usr/bin/", ava[1]);
  execute(cp + 4, &ava[1]);
  execute(cp, &ava[1]);
  write(STDOUT, "Command not found\n", 19);
  
  return 0;
}
