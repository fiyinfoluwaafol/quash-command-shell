#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h> // For PATH_MAX

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

char delimiters[] = " \t\r\n";
extern char **environ;

int main() {
    // Stores the string typed into the command line.
    char command_line[MAX_COMMAND_LINE_LEN];
    // Buffer to store the current working directory.
    char cwd[PATH_MAX];

    // Stores the tokenized command line input.
    char *arguments[MAX_COMMAND_LINE_ARGS];

    while (true) {

        do {
            // Get current working directory and print the shell prompt.
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s> ", cwd);
            } else {
                perror("getcwd() error");
                exit(1);
            }
            fflush(stdout);

            // Read input from stdin and store it in command_line.
            if ((fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL) && ferror(stdin)) {
                fprintf(stderr, "fgets error");
                exit(0);
            }

        } while (command_line[0] == '\n');  // Repeat if just ENTER is pressed

        // Remove the trailing newline character from the command line input.
        if (command_line[strlen(command_line) - 1] == '\n') {
            command_line[strlen(command_line) - 1] = '\0';
        }

        // If the user input was EOF (ctrl+d), exit the shell.
        if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            fflush(stderr);
            exit(0);
        }

        // Tokenize the command line input.
        int arg_count = 0;
        char *token = strtok(command_line, delimiters);
        while (token != NULL && arg_count < MAX_COMMAND_LINE_ARGS - 1) {
            // Handle environment variables starting with '$'
            if (token[0] == '$') {
                char *env_var = getenv(token + 1);  // Skip the '$' character
                if (env_var != NULL) {
                    arguments[arg_count] = env_var;
                } else {
                    arguments[arg_count] = "";  // Empty string if variable not found
                }
            } else {
                arguments[arg_count] = token;
            }
            arg_count++;
            token = strtok(NULL, delimiters);
        }
        arguments[arg_count] = NULL;  // Null-terminate the arguments array

        // Continue if no command was entered
        if (arg_count == 0) {
            continue;
        }

        // Built-in command: exit
        if (strcmp(arguments[0], "exit") == 0) {
            exit(0);
        }
        // Built-in command: cd
        else if (strcmp(arguments[0], "cd") == 0) {
            if (arg_count < 2) {
                fprintf(stderr, "cd: missing argument\n");
            } else {
                if (chdir(arguments[1]) != 0) {
                    perror("cd error");
                }
            }
            continue;
        }
        // Built-in command: pwd
        else if (strcmp(arguments[0], "pwd") == 0) {
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                perror("getcwd() error");
            }
            continue;
        }
        // Built-in command: echo
        else if (strcmp(arguments[0], "echo") == 0) {
            int i; // Declare i outside the for loop
            for (i = 1; i < arg_count; i++) {
                printf("%s ", arguments[i]);
            }
            printf("\n");
            continue;
        }
        // Built-in command: env
        else if (strcmp(arguments[0], "env") == 0) {
            int i; // Declare i outside the loop
            if (arg_count == 1) {
                // Print all environment variables
                char **env; // Declare env outside the loop
                for (env = environ; *env != 0; env++) {
                    printf("%s\n", *env);
                }
            } else {
                // Print specific environment variables
                for (i = 1; i < arg_count; i++) {
                    char *env_var = getenv(arguments[i]);
                    if (env_var != NULL) {
                        printf("%s=%s\n", arguments[i], env_var);
                    } else {
                        printf("%s not found\n", arguments[i]);
                    }
                }
            }
            continue;
        }
        // Built-in command: setenv
        else if (strcmp(arguments[0], "setenv") == 0) {
            if (arg_count < 3) {
                fprintf(stderr, "setenv: missing arguments\n");
            } else {
                if (setenv(arguments[1], arguments[2], 1) != 0) {
                    perror("setenv error");
                }
            }
            continue;
        }

        // TODO: Implement external commands in Task 2

    }
    // This should never be reached.
    return -1;
}