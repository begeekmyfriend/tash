

         TASH -- Thompson-Again SHell

  TASH is a mini shell deprived from Unix V6SH which used to be refferred as to
Thompson shell (http://en.wikipedia.org/wiki/Thompson_shell) which was written
by Ken Thompson in 1971. I did some little things in ANSI C and ported it on
modern Linux system like Ubuntu, CentOS and Fedora. It is still regarded as a
quite simple CLI interpreter but not yet for scripting.

  The basic functions of this interpreter include command line, pipe, terminal
reporting, I/O redirection and so on. More information is mentioned at on-line
manual: http://v6shell.org/history/sh.1.html.

  The tash currently contains only two source files: tash.c glob.c. You may make
them into executable files at a time and install them on /usr/local/bin as for
your user id as root.
  
  You may have a quick start inputing command lines in three ways as follows:

  1. leo@leo-desktop:~/tash$ ./tash -c "ls *.c"
     glob.c tash.c

  2. leo@leo-desktop:~/tash$ ./tash -t test.sh
     ls *.c
     glob.c tash.c

  3. leo@leo-desktop:~/tash$ cat test.sh
     #!/usr/local/bin/tash
     ls *.c
     leo@leo-desktop:~/tash$ chmod +x test.sh
     leo@leo-desktop:~/tash$ ./test.sh
     glob.c tash.c  
     
  However, there are still some problems in symbol parsing such as pipe function.
For example, when you input command lines like that:

  ./tash -c "ls | grep *.c"

  It will prompt "Grep: (Standard input): Bad file descriptor..." I guess it
must be problems with standard I/O redirection, but I do not know how. It would
be grateful if someone would like to point out and fix this bug.

  The original source code and mannual of Unix V6SH was put on The Unix Heritage
Society(TUHS). You may download them at http://minnie.tuhs.org/cgi-bin/utree.pl
for free.

  Any good suggestions are welcomed:^)
