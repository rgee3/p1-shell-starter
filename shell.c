/**
 * Ruth Gee, CSS 430, P1-Unix Shell
*
* This program implements a UNIX shell in C.
 * It executes user commands, supports piping (|), input/output redirection (<, >),
 * and background execution (&).
 * It uses fork(), execvp(), waitpid(), pipe(), and dup2() for process management and communication.
*/

#include "shell.h"

char lastCommand[MAXLINE] = "";

int main(int argc, char **argv) {
  if (argc == 2 && equal(argv[1], "--interactive")) {
    return interactiveShell();
  } else {
    return runTests();
  }
}

// interactive shell to process commands
int interactiveShell() {
  bool should_run = true;
  char *line = calloc(1, MAXLINE);
  while (should_run) {
    printf(PROMPT);
    fflush(stdout);
    int n = fetchline(&line);
    printf("read: %s (length = %d)\n", line, n);
    // ^D results in n == -1
    if (n == -1 || equal(line, "exit")) {
      should_run = false;
      continue;
    }
    if (equal(line, "")) {
      continue;
    }
    processLine(line);
  }
  free(line);
  return 0;
}

/** Splits a command string into an array of arguments; tokenizes the input
 * command using whitespace as delimiters.  Stores tokens in the args[]
 * array -- used for executing the command via execvp().
 * @param command -- command string to be tokenized
 * @param args[]  Array to store arguments
 */
void tokenizeCommand(char *command, char *args[]) {
  int i = 0;
  char *tokenPtr;
  char *token = strtok_r(command, " \t\n", &tokenPtr);

  // Extract arguments
  while (token != NULL && i < MAXLINE / 2) {
    // Skip '&' for now
    if (strcmp(token, "&") != 0) {
      args[i++] = token;
    }
    token = strtok_r(NULL, " \t\n", &tokenPtr);
  }
  args[i] = NULL; // NULL terminates args array for execvp()
}

/** Forks a child process to execute a shell command.  First checks for
 * pipes (`|`) and, if found, creates a pipe and forks two child processes
 * to execute the left and right commands. If no pipe, it handles normal
 * execution, including redirection (input '<' and output '>') and background
 * execution ('&').
 *
 * @param command -- command string to execute
 * @param should_wait -- true if parent should wait for child to finish
 */
void executeCommand(char *command, bool should_wait) {
  // Special output for 'ascii' command as per assignment instructions
  if (strcmp(command, "ascii") == 0) {
    printf("\n  |\\_/|        ****************************    (\\_/)\n");
    printf(" / @ @ \\       *  \"Purrrfectly pleasant\"  *   (='.'=)\n");
    printf("( > º < )      *       Poppy Prinz        *   (\")_(\")\n");
    printf(" `>>x<<´       *   (pprinz@example.com)   *\n");
    printf(" /  O  \\       ****************************\n\n");
    return; // no fork necessary
  }

  char *tokenPtr;
  char *leftCommand = strtok_r(command, "|", &tokenPtr);
  char *rightCommand = strtok_r(NULL, "|", &tokenPtr);

  if (rightCommand != NULL) { // Pipe detected
    while (*rightCommand == ' ') { // Trim leading spaces
      rightCommand++;
    }

    // Create pipe
    int pipefd[2];
    if (pipe(pipefd) < 0) {
      perror("Pipe failed");
      return;
    }

    // Fork first child, executes left side of pipe
    pid_t pid1 = fork();
    if (pid1 < 0) {
      perror("Fork failed");
      return;
    }

    if (pid1 == 0) { // First child process -- left command
      close(pipefd[RD]);
      dup2(pipefd[WR], STDOUT_FILENO);
      close(pipefd[WR]);

      char *args[MAXLINE / 2 + 1];
      tokenizeCommand(leftCommand, args);
      if (args[0] == NULL) {
        exit(1);
      }

      execvp(args[0], args);
      perror("execvp failed");
      exit(1);
    }

    // Fork second child, executes right side of pipe
    pid_t pid2 = fork();
    if (pid2 < 0) {
      perror("Fork failed");
      return;
    }

    if (pid2 == 0) { // Second child process (right command)
      close(pipefd[WR]);
      dup2(pipefd[RD], STDIN_FILENO);
      close(pipefd[RD]);

      char *args[MAXLINE / 2 + 1];
      tokenizeCommand(rightCommand, args);
      if (args[0] == NULL) {
        exit(1);
      }

      execvp(args[0], args);
      perror("execvp failed");
      exit(1);
    }

    close(pipefd[WR]);
    waitpid(pid1, NULL, 0);
    close(pipefd[RD]);
    waitpid(pid2, NULL, 0);
    return;
  }

  // No pipe, process normally
  char *args[MAXLINE / 2 + 1];
  tokenizeCommand(command, args);
  if (args[0] == NULL) {
    return;
  }

  char *input_file = NULL;
  char *output_file = NULL;

  // Check for input '<' and output '>' redirection
  for (int i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], "<") == 0 && args[i + 1] != NULL) { // input redirection
      input_file = args[i + 1];
      args[i] = NULL;
    } else if (strcmp(args[i], ">") == 0 && args[i + 1] != NULL) { // output redirection
      output_file = args[i + 1];
      args[i] = NULL;
    }
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("Fork failed");
    return;
  }

  if (pid == 0) { // Child process
    if (input_file != NULL) {
      int fd = open(input_file, O_RDONLY);
      if (fd < 0) {
        perror("Error opening input file");
        exit(1);
      }
      dup2(fd, STDIN_FILENO); // Redirect stdin
      close(fd);
    }

    if (output_file != NULL) {
      int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        perror("Error opening output file");
        exit(1);
      }
      dup2(fd, STDOUT_FILENO); // redirect stdout
      close(fd);
    }

    execvp(args[0], args);
    perror("execvp failed");
    exit(1);
  }

  // Handle background execution -- '&'
  if (!should_wait) {
    printf("[Background PID: %d] %s\n", pid, args[0]);
    return; // do not wait
  }

  waitpid(pid, NULL, 0);
}

/** Parses and executes user command line.  Each command is stored in 'commands[]' array,
 * along with its background flag.
 * -- '!!' executes last command if entered
 * -- Splits multiple commands using ';'
 * -- Handles background execution ('&')
 * @param line -- input command string
 */
void processLine(char *line) {
  char *original_line = strdup(line);   // Store original command for handling history
  printf("processing line: %s\n", original_line);

  // Handle command history ('!!') -- execute last command
  if (strcmp(line, "!!") == 0) {
    if (strlen(lastCommand) == 0) {
      printf("No commands in history.\n");
      free(original_line);
      return;
    } else {
      printf("Executing: %s\n", lastCommand);
      strcpy(line, lastCommand);  // replace '!!' with last command
    }
  } else {
    strcpy(lastCommand, original_line); // store command in history
  }

  char *command_copy = strdup(line);
  char *commands[MAXLINE]; // Store individual commands
  int cmd_count = 0;
  bool background_flags[MAXLINE] = {false}; // tracks '&'

  // Split by semicolon to handle multiple commands
  char *tokenPtr;
  char *cmd = strtok_r(command_copy, ";", &tokenPtr);

  while (cmd != NULL && cmd_count < MAXLINE) {
    while (*cmd == ' ' || *cmd == '\t') { // trim leading spaces
      cmd++;
    }
    // trim trailing spaces
    char *end = cmd + strlen(cmd) - 1;
    while (end > cmd && (*end == ' ' || *end == '\t'))
      end--;
    *(end + 1) = '\0';

    if (*cmd != '\0') {
      // Check for background execution (`&`)
      char *ampersand = strstr(cmd, "&");
      if (ampersand != NULL) {
        *ampersand = '\0'; // Remove `&`
        while (*(ampersand + 1) == ' ' || *(ampersand + 1) == '\t') {
          ampersand++;
        }

        // Store first part as background command
        background_flags[cmd_count] = true;
        commands[cmd_count++] = strdup(cmd);

        ampersand++; // Move past `&`

        // If another command after `&`, add as foreground command
        if (*ampersand != '\0') {
          commands[cmd_count] = strdup(ampersand);
          background_flags[cmd_count] = false;
          cmd_count++;
        }
      } else {
        // If no `&`, treat as normal
        commands[cmd_count++] = strdup(cmd);
      }
    }
    cmd = strtok_r(NULL, ";", &tokenPtr);
  }

  for (int i = 0; i < cmd_count; i++) {
    printf("Executing: [%s] Background: [%d]\n", commands[i], // Used for debugging
           background_flags[i]);
  }

  // Execute commands in sequence
  for (int i = 0; i < cmd_count; i++) {
    executeCommand(commands[i], !background_flags[i]);

    if (background_flags[i]) {
      usleep(10000);
    }
  }

  free(command_copy);
  free(original_line);
}

int runTests() {
  printf("*** Running basic tests ***\n");
  char lines[7][MAXLINE] = {
      "ls",      "ls -al", "ls & whoami ;", "ls > junk.txt", "cat < junk.txt",
      "ls | wc", "ascii"};
  for (int i = 0; i < 7; i++) {
    printf("* %d. Testing %s *\n", i + 1, lines[i]);
    processLine(lines[i]);
  }

  return 0;
}

// return true if C-strings are equal
bool equal(char *a, char *b) { return (strcmp(a, b) == 0); }

// read a line from console
// return length of line read or -1 if failed to read
// removes the \n on the line read
int fetchline(char **line) {
  size_t len = 0;
  size_t n = getline(line, &len, stdin);
  if (n > 0) {
    (*line)[n - 1] = '\0';
  }
  return n;
}
