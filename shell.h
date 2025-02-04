/**
* Ruth Gee, CSS 430, P1-Unix Shell
*
* This program implements a UNIX shell in C.
* It executes user commands, supports piping (|), input/output redirection (<, >),
* and background execution (&).
* It uses fork(), execvp(), waitpid(), pipe(), and dup2() for process management and communication.
*/

#ifndef SHELL_H
#define SHELL_H

#include <assert.h>  // assert
#include <fcntl.h>   // O_RDWR, O_CREAT
#include <stdbool.h> // bool
#include <stdio.h>   // printf, getline
#include <stdlib.h>  // calloc
#include <string.h>  // strcmp
#include <unistd.h>  // execvp
#include <sys/wait.h>
#include <ctype.h>

#define MAXLINE 80
#define PROMPT "osh> "

#define RD 0
#define WR 1

extern char lastCommand[MAXLINE];

bool equal(char *a, char *b);
int fetchline(char **line);
int interactiveShell();
int runTests();
void tokenizeCommand(char *command, char *args[]);
void executeCommand(char *command, bool should_wait);
void processLine(char *line);
int main();

#endif