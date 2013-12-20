/***************************************************
 *  File: tash.c -- A mini command line interpreter.
 *
 *          Author      Year    Description
 *
 *   1.  Ken Thompson   1975    Create on Unix V6
 *   2.  Leo Ma         2013    Porting on Linux
 *
 ***************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

//
//#define SEEK_SET  0
//#define SEEK_CUR  1
//#define SEEK_END  2
//
//#define SIGINT   2 
//#define SIGQUIT  3
//
//#define ENOMEM  12 
//#define ENOEXEC  8
//
//#define SIG_DFL  ((void(*)(int))0)
//#define SIG_IGN  ((void(*)(int))1)

#define STDIN   0
#define STDOUT  1
#define STDERR  2

#define LINSIZ  256
#define TOKSIZ  50
#define TRESIZ  100

#define QUOTE  0x80 

// Attribute
#define FAND  1
#define FCAT  2
#define FPIN  4
#define FPOU  8
#define FPAR  16
#define FINT  32
#define FPRS  64

// Type
#define TCOM  1 
#define TPAR  2
#define TFIL  3
#define TLST  4

// Field
#define DTYP  0
#define DLEF  1
#define DRIT  2
#define DFLG  3
#define DSPR  4
#define DCOM  5

// '$' indicator
char *dolp;
char **dolv;
int dolc;

char pidp[4];

char *prompt;

// Line buffer
char line[LINSIZ];
char *linep;
char *elinep;

// Token list
char *toks[TOKSIZ];
char **tokp;
char **etokp;

// Syntax tree
unsigned trebuf[TRESIZ];
unsigned *treep;
unsigned *treeend;

char peekc;

char overflow;
char glob;
char error;
char uid;
char setintr;

char *arginp;
int onelflg;


char *mesg[] = {
  0,
  "Hangup",
  0,
  "Quit",
  "Illegal instruction",
  "Trace/BPT trap",
  "IOT trap",
  "EMT trap",
  "Floating exception",
  "Killed",
  "Bus error",
  "Memory fault",
  "Bad system call",
  0,
  "Sig 14",
  "Sig 15",
  "Sig 16",
  "Sig 17",
  "Sig 18",
  "Sig 19",
};

struct stime {
  int proct[2];
  int cputim[2];
  int systim[2];
} timeb;


void put(char c)
{
  write(STDOUT, &c, 1);
}

void prs(char *s)
{
  while (*s != '\0')
    put(*s++);
}

void prn(int n)
{
  char c[11];
  char *p;
  char sign;

  if (n < 0) {
    n = -n;
    sign = '-';
  } else {
    sign = '\0';
  }

  p = c;
  while (n > 0) {
    *p++ = n % 10 + '0';
    n /= 10;
  }
  
  if (sign != '\0')
    put(sign);
  
  while (p >= c) {
      put(*--p);
  }
}

void err(char *s)
{
  prs(s);
  prs("\n");
  if (!prompt) {
    lseek(STDIN, 0, SEEK_END);
    exit(-1);
  }
}

int any(int c, char *s)
{
  while (*s) {
    if (c == *s++)
      return 1;
  }
  return 0;
}

int equal(char *s1, char *s2)
{
  while (*s1 == *s2++) {
    if (*s1++ == '\0')
      return 1;
  }
  return 0;
}

void scan(unsigned *at, char (*f)(char))
{
  char *p, c;
  unsigned *t;

  t = at + DCOM;
  while ((p = (char *)*t++) != '\0')
    while ((c = *p) != '\0')
      *p++ = (*f)(c);
}

char tglob(char c)
{
  if (any(c, "[?*"))
    glob = 1;
  return c;
}

char trim(char c)
{
  return (c & 0x7f);
}

unsigned *tree(int n)
{
  unsigned *t;
  
  t = treep;
  treep += n;
  if (treep > treeend) {
    prs("Command line overflow\n");
    error++;
    //reset();
    exit(-1);
  }
  return t;
}

unsigned *parse1(char **p1, char **p2);
unsigned *parse2(char **p1, char **p2);
unsigned *parse3(char **p1, char **p2);

unsigned *parse(char **p1, char **p2)
{
  while (p1 != p2) {
    if (any(**p1, ";&\n")) {
      p1++;
    } else {
      return parse1(p1, p2);
    }
  }
  return NULL;
}

/* Stage 1 parses command list whose left and right trees store its subcommand,
 * while left tree to be passed down to the next stage and right tree to be recursed.
 */
unsigned *parse1(char **p1, char **p2)
{
  char **p, c;
  unsigned *t, *t1;
  int l;

  l = 0;
  for (p = p1; p != p2; p++) {
    switch (**p) {
      case '(':
        l++;
        continue;
      case ')':
        if (--l < 0)
          error++;
        continue;
      case '&':
      case ';':
      case '\n':
        // Parathesis should be passed down to the next stage.
        if (l == 0) {
          c = **p;
          t = tree(4);
          t[DTYP] = TLST;
          t[DLEF] = (unsigned)parse2(p1, p);
          t[DFLG] = 0;
          // Push down the attribute immediately.
          if (c == '&') {
            t1 = (unsigned *)t[DLEF];
            t1[DFLG] = FAND | FPRS | FINT;
          }
          t[DRIT] = (unsigned)parse(p + 1, p2);
        }
        return t;
    }
  }

  // Not found, transfer into the next stage.
  // Note: we should check whether parathesis is complete or not. 
  if (l == 0) {
    return parse2(p1, p2);
  } else {
    error++;
    return NULL;
  }
}

/* Stage 2 parses filter whose left tree to be transferred to stage 3
 * and right tree to be recursed. 
 */
unsigned *parse2(char **p1, char **p2)
{
  char **p;
  unsigned *t;
  int l;

  l = 0;
  for (p = p1; p != p2; p++) {
    switch (**p) {
      case '(':
        l++;
        continue;
      case ')':
        if (--l < 0)
          error++;
        continue;
      case '|':
      case '^':
        // Here we pass parathesis down to the next stage.
        if (l == 0) {
          t = tree(4);
          t[DTYP] = TFIL;
          t[DLEF] = (unsigned)parse3(p1, p);
          t[DRIT] = (unsigned)parse2(p + 1, p2);
          t[DFLG] = 0;  // Note: Attribute to be pushed down in execute() function.
          return t;
        }
    }
  }

  /* Here we do not need to check whether parathesis is complete (l == 0)
   * for we are in the middle stage.
   */
  return parse3(p1, p2);
}

/* Stage 3 parses parathesis command which should be transferred to stage 1
 * and simple command which to be generated directly.
 */
unsigned *parse3(char **p1, char **p2)
{
  char **p, c;
  char **lp, **rp;
  unsigned *t;
  int n, l;
  unsigned input, output, flag;

  flag = 0;
  // Last sub command in parathesis commands.
  if (**p2 == ')') {
    flag |= FPAR;
  }

  lp = NULL;
  rp = NULL;
  input = 0;
  output = 0;
  n = 0;
  l = 0;

  for (p = p1; p != p2; p++) {
    switch (c = **p) {

      case '(':
        if (l == 0) {
          if (lp != NULL)
            error++;
          lp = p + 1;
        }
        l++;
        continue;

      case ')':
        if (--l == 0)
          rp = p;
        continue;

      case '>':
        p++;
        if (p != p2 && **p == '>') {
          flag |= FCAT;
        } else {
          p--;
        }
        // Note: Here's no break!
      case '<':
        if (l == 0) {
          p++;
          // No character
          if (p == p2) {
            error++;
            p--;
          }
          // Illegal character
          if (any(**p, "<>(")) {
            error++;
          }
          // Input&output redirection
          if (c == '<') {
            if (input != 0)
              error++;
            input = (unsigned)*p;
          } else {
            if (output != 0) {
              error++;
            }
            output = (unsigned)*p;
          }
        }
        continue;

      default:
        // Simple command, store command arguments
        if (l == 0) {
          p1[n++] = *p;
        }
    }
  }

  // Parathesis command
  if (lp != 0) {
    if (n != 0)
      error++;
    t = tree(5);
    t[DTYP] = TPAR;
    t[DSPR] = (unsigned)parse1(lp, rp);
    goto OUT;
  }

  // Simple command
  if (n == 0)
    error++;
  p1[n++] = NULL;
  t = tree(n + DCOM);
  t[DTYP] = TCOM;
  for (l = 0; l < n; l++)
    t[l + DCOM] = (unsigned)p1[l];
  t[l + DCOM] = 0;  // pointer to NULL
OUT:
  t[DFLG] = flag;
  t[DLEF] = input;
  t[DRIT] = output;
  return t;
}

void texec(char *path, unsigned *t)
{
  extern int errno;

  execv(path, (char **)(t + DCOM));

  if (errno == ENOEXEC) {
    if (*linep != '\0')
      t[DCOM] = (unsigned)linep;
    t[DSPR] = (unsigned)"/bin/sh";
    execv((char *)t[DSPR], (char **)(t + DSPR));
    prs("No shell!\n");
    exit(-1);
  }
  
  if (errno == ENOMEM) {
    prs((char *)t[DCOM]);
    err(": too large");
    exit(-1);
  }
}

void pwait(int p, unsigned *t)
{
  int pid, error, status;

  if (p == 0)
    return;

  for (;;) {
    pid = wait(&status);
    if (pid == -1)
      break;
    error = status & 0x7f;
    if (mesg[error]) {
      if (pid != p) {
        prn(pid);
        prs(": ");
      }
      prs(mesg[error]);
      if (status & 0x80)
        prs(" -- Core dumped\n");
    }

    if (error)
      err("");

    if (pid == p)
      break;
  }
}

void execute(unsigned *t, int *pf1, int *pf2)
{
  unsigned flag;
  unsigned *t1;
  char *cp1, *cp2;
  int pid, fd, pv[2];
  extern int errno;

  if (t == NULL)
    return;

  switch (t[DTYP]) {

    case TCOM:
      cp1 = (char *)t[DCOM];
      /* Buid-in command */
      if (equal(cp1, "chdir")) {
        if (t[DCOM + 1]) {
          if (chdir((char *)t[DCOM + 1]) < 0)
            err("chdir: bad directory");
        } else {
          err("chdir: arg count");
        }
        return;
      }
      
      if (equal(cp1, "shift")) {
        if (dolc < 1) {
          prs("shift: arg count\n");
          return;
        }
        dolv[1] = dolv[0];
        dolv++;
        dolc--;
        return;
      }

      if (equal(cp1, "login")) {
        if (prompt) {
          execv("/bin/login", (char **)(t + DCOM));
        }
        prs("login: cannot execte\n");
        return;
      }
  
      if (equal(cp1, "newgrp")) {
        if (prompt) {
          execv("/bin/newgrp", (char **)(t + DCOM));
        }
        prs("newgrp: cannot execte\n");
        return;
      }
  
      if (equal(cp1, "wait")) {
        pwait(-1, 0);
        return;
      }

      if (equal(cp1, ":"))
        return;

    // Note: Here's no break! self-defined command below
    case TPAR:
      flag = t[DFLG];
      pid = 0;
      if (!(flag & FPAR))
        pid = fork();
      if (pid == -1) {
        err("try again");
        return;
      }

      // Parent process (shell): NON-FPAR will run here
      if (pid != 0) {
        // FPIN will close pipe descriptor pv[2].
        if (flag & FPIN) {
          close(pf1[0]);
          close(pf1[1]);
        }
        // Print PID
        if (flag & FPRS) {
          prn(pid);
          prs("\n");
        }
        // No wait
        if (flag & FAND)
          return;
        // Note: we do not close FPOU's pv[2] here for it is to be reused by FPIN sequently.
        if (!(flag & FPOU))
          pwait(pid, t);

        return;
      }

      // Redirect STDIN
      if (t[DLEF]) {
        fd = open((char *)t[DLEF], 0);
        if (fd < 0) {
          prs((char *)t[DLEF]);
          err(": cannot open");
          exit(-1);
        }
        //close(STDIN);
        dup2(fd, STDIN);
        close(fd);
      }

      // Redirect STDOUT
      if (t[DRIT]) {
        if (flag & FCAT) {
          fd = open((char *)t[DRIT], 1);
          if (fd >= 0) {
            lseek(fd, 0, SEEK_END);
            goto f1;
          }
        }
        fd = creat((char *)t[DRIT], 0666);
        if (fd < 0) {
          prs((char *)t[DRIT]);
          err(": cannot creat");
          exit(-1);
        }
f1:
        //close(STDOUT);
        dup2(fd, STDOUT);
        close(fd);
      }

      // Redirect pipe in
      if (flag & FPIN) {
        //close(STDIN);
        dup2(pf1[0], STDIN);
        close(pf1[0]);
        close(pf1[1]);
      }

      // Redirect pipe out
      if (flag & FPOU) {
        //close(STDOUT);
        dup2(pf2[1], STDOUT);
        close(pf2[0]);
        close(pf2[1]);
      }

      // Ignore interrupt && no input && non pipe in, redirect STDIN to /dev/null
      // STDIN may be forked from parent.
      if ((flag & FINT) && !t[DLEF] && !(flag & FPIN)) {
        fd = open("/dev/null", 0);
        //close(STDIN);
        dup2(fd, STDIN);
        close(fd);
      }

      if (!(flag & FINT) && setintr) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
      }

      // TPAR recursive, exit immediately
      if (t[DTYP] == TPAR) {
        // Push down attribute
        if ((t1 = (unsigned *)t[DSPR]) != NULL)
          t1[DFLG] |= flag & FINT;
        execute(t1, NULL, NULL);
        exit(0);
      }

      glob = 0;
      scan(t, &tglob);
      if (glob) {
        t[DSPR] = (unsigned)"glob";//"/etc/glob";
        execv((char *)t[DSPR], (char **)(t + DSPR));
        prs("glob: cannot execute\n");
        exit(-1);
      }

      scan(t, &trim);
      *linep = '\0';
      texec((char *)t[DCOM], t);
      cp1 = linep;
      cp2 = "/usr/bin/";
      while ((*cp1 = *cp2++) != '\0')
        cp1++;
      cp2 = (char *)t[DCOM];
      while ((*cp1++ = *cp2++) != '\0')
        continue;
      texec(linep + 4, t);
      texec(linep, t);
      prs((char *)t[DCOM]);
      err(": not found");
      exit(-1);

    // Note: Here's no break!
    case TFIL:
      flag = t[DFLG];
      pipe(pv);
      // Push down filter attribute
      t1 = (unsigned *)t[DLEF];
      t1[DFLG] |= FPOU | (flag & (FPIN | FINT | FPRS));
      execute(t1, pf1, pv);
      t1 = (unsigned *)t[DRIT];
      t1[DFLG] |= FPIN | (flag & (FPOU | FINT | FAND | FPRS));
      execute(t1, pv, pf2);
      return;

    case TLST:
      // Push down list attribute.
      flag = t[DFLG] | FINT;
      if ((t1 = (unsigned *)t[DLEF]) != NULL)
        t1[DFLG] |= flag;
      execute(t1, NULL, NULL);
      if ((t1 = (unsigned *)t[DRIT]) != NULL)
        t1[DFLG] |= flag;
      execute(t1, NULL, NULL);
      return;
  }
}

int readc()
{
  int cc;
  char c;
  
  // Option -c
  if (arginp) {
    if (arginp == (void *)1)
      exit(0);
    if ((c = *arginp++) == '\0') {
      c = '\n';
      arginp = (void *)1;
    }
    return (int)c;
  }

  // Option -t
  if (onelflg == 1)
    exit(0);
  if (read(STDIN, &cc, 1) != 1)
    exit(-1);
  if (cc == '#')
    while (cc != '\n')
      if (read(STDIN, &cc, 1) != 1)
        exit(-1);
  if (cc == '\n' && onelflg)
    onelflg--;

  return cc;
}

char getch()
{
  char c;

  if (peekc) {
    c = peekc;
    peekc = 0;
    return c;
  }
  
  if (tokp > etokp) {
    tokp -= 10;
    while ((c = getch()) != '\n')
      continue;
    tokp += 10;
    err("Too many toks");
    overflow++;
    return c;
  }
  
  if (linep > elinep) {
    linep -= 10;
    while ((c = getch()) != '\n')
      continue;
    linep += 10;
    err("Too many characters");
    overflow++;
    return c;
  }

GET:
  // '$'
  if (dolp) {
    c = *dolp++;
    if (c != '\0')
      return c;
    dolp = 0;
  }

  c = readc();

  // '\'
  if (c == '\\') {
    c = readc();
    if (c == '\n')
      return ' ';
    return (c | QUOTE);
  }

  if (c == '$') {
    c = readc();
    // '$n'
    if (c >= '0' || c <= '9') {
      if (c - '0' < dolc)
        dolp = dolv[c - '0'];
      goto GET;
    }
    // '$$'
    if (c == '$') {
      dolp = pidp;
      goto GET;
    }
  }

  return (c & 0x7f);
}


void token()
{
  char c, c1;

  *tokp++ = linep;
  
TOKEN:
  switch (c = getch()) {
    put(c);
    case '\t':
    case ' ':
      goto TOKEN;

    case '\'':
    case '"':
      c1 = c;
      // Note: Here we should read directly from input stream!
      while ((c = readc()) != c1) {
        if (c == '\n') {
          error++;
          peekc = c;  // '\n' should be pushed back for next session
          return;
        }
        *linep++ = c | QUOTE;
      }
      goto SEPERATE;

    case '&':
    case ';':
    case '<':
    case '>':
    case '(':
    case ')':
    case '|':
    case '^':
    case '\n':
      *linep++ = c;
      *linep++ = '\0';
      return;
  }

  // Push back non meta character
  peekc = c;

SEPERATE:
  for (;;) {
    c = getch();
    // Here is token seperator.
    if (any(c, " '\"\t;&<>()|^\n")) {
      peekc = c;  // Push back as next token
      if (any(c, "\"'"))
        goto TOKEN;
      *linep++ = '\0';
      return;
    }
    *linep++ = c;
  }
}

void session()
{
  char *cp;
  unsigned *t;

  tokp = toks;
  etokp = toks + TOKSIZ - 5;
  linep = line;
  elinep = line + LINSIZ - 5;
  error = 0;
  overflow = 0;
  
  // End of one session when the first character of line buffer is '\n'
  do {
    cp = linep;
    token();
  } while (*cp != '\n');

  treep = trebuf;
  treeend = (unsigned *)trebuf + TRESIZ;

  if (!overflow) {
    if (error == 0) {
      //setexit();
      //if (error)
        //return;
      t = parse(toks, tokp);
    }

    if (error) {
      err("Syntax error!");
    } else {
      execute(t, NULL, NULL);
    }
  }
}

int main(int argc, char **argv)
{
  int i, pid;

  for (i = STDERR; i < 16; i++)
    close(i);
  dup2(STDOUT, STDERR);
  for (i = 4; i >= 0; i--) {
    pidp[i] = pid % 10 + '0';
    pid /= 10;
  }

  prompt = "% ";
  if ((uid = getuid()) == 0)
    prompt = "# ";

  if (argc > 1) {
    prompt = NULL;
    if (*argv[1] == '-') {
      **argv = '-';
      if (argv[1][1] == 'c' && argc > 2)
        arginp = argv[2];
      else if (argv[1][1] == 't')
        onelflg = 2;
    } else {
      i = open(argv[1], 0);
      if (i < 0) {
        prs(argv[1]);
        err(": cannot open");
      }
      //close(STDIN);
      dup2(i, STDIN);
      close(i);
    }
  }

  if (**argv == '-') {
    setintr++;
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
  }

  dolv = argv + 1;
  dolc = argc - 1;

  for (;;) {
    if (prompt != 0)
      prs(prompt);
    peekc = getch();  // Pre-read one character
    session();
  }

  return 0;
}
