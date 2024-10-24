#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

char delimiters[] = " \t\r\n";
extern char **environ;

void sigint_handler(int sig) {
    // Ignore SIGINT in the shell process
    printf("\n");
}

void sigalrm_handler(int sig) {
    // Handle SIGALRM in the child process
    printf("Process exceeded time limit and was killed.\n");
    exit(1);
}

int remove_quotes(char *arg) {
    int len = strlen(arg);
    if (arg[0] == '"' && arg[len - 1] == '"') {
        // Remove surrounding quotes by shifting the string
        memmove(arg, arg + 1, len - 2);
        arg[len - 2] = '\0';
        return 1;
    }
    return 0;
}

void expand_variables(char **args, int arg_count) {
    int i;
    for (i = 0; i < arg_count; i++) {
        if (args[i][0] == '$') {
            // Get the environment variable without the '$'
            char *env_value = getenv(args[i] + 1);
            if (env_value) {
                args[i] = env_value;
            } else {
                args[i] = "";
            }
        }
    }
}

int main() {
    char command_line[MAX_COMMAND_LINE_LEN];
    char *arguments[MAX_COMMAND_LINE_ARGS];
    int i;

    // Set up signal handler for Ctrl-C
    signal(SIGINT, sigint_handler);

    while (true) {
        // Print hardcoded shell prompt
        printf("/home/codio> ");
        fflush(stdout);

        // Read input from stdin
        if ((fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL) && ferror(stdin)) {
            fprintf(stderr, "fgets error\n");
            exit(1);
        }

        // Remove trailing newline character
        if (command_line[strlen(command_line) - 1] == '\n') {
            command_line[strlen(command_line) - 1] = '\0';
        }

        // Exit on EOF (Ctrl-D)
        if (feof(stdin)) {
            printf("\n");
            exit(0);
        }

        // Skip empty commands
        if (strlen(command_line) == 0) {
            continue;
        }

        // Tokenize the command line input
        char *token = strtok(command_line, delimiters);
        int arg_count = 0;
        int background = 0;
        int redirect_output = 0;
        int redirect_input = 0;
        int pipe_present = 0;
        char *output_file = NULL;
        char *input_file = NULL;
        char *pipe_args[MAX_COMMAND_LINE_ARGS];
        int pipe_arg_count = 0;

        while (token != NULL && arg_count < MAX_COMMAND_LINE_ARGS - 1) {
            if (strcmp(token, "&") == 0) {
                background = 1;
            } else if (strcmp(token, ">") == 0) {
                redirect_output = 1;
                token = strtok(NULL, delimiters);
                if (token != NULL) {
                    output_file = token;
                } else {
                    fprintf(stderr, "Syntax error: expected output file after '>'\n");
                    break;
                }
            } else if (strcmp(token, "<") == 0) {
                redirect_input = 1;
                token = strtok(NULL, delimiters);
                if (token != NULL) {
                    input_file = token;
                } else {
                    fprintf(stderr, "Syntax error: expected input file after '<'\n");
                    break;
                }
            } else if (strcmp(token, "|") == 0) {
                pipe_present = 1;
                arguments[arg_count] = NULL;
                token = strtok(NULL, delimiters);
                while (token != NULL && pipe_arg_count < MAX_COMMAND_LINE_ARGS - 1) {
                    remove_quotes(token);
                    if (token[0] == '$') {
                        char *env_var = getenv(token + 1);
                        pipe_args[pipe_arg_count++] = env_var ? env_var : "";
                    } else {
                        pipe_args[pipe_arg_count++] = token;
                    }
                    token = strtok(NULL, delimiters);
                }
                pipe_args[pipe_arg_count] = NULL;
                break;
            } else if (token[0] == '$') {
                char *env_var = getenv(token + 1);
                arguments[arg_count++] = env_var ? env_var : "";
            } else {
                remove_quotes(token);
                arguments[arg_count++] = token;
            }
            token = strtok(NULL, delimiters);
        }
        arguments[arg_count] = NULL;

        if (arguments[0] == NULL) {
            continue;
        }

        // Implement built-in commands
        if (strcmp(arguments[0], "cd") == 0) {
            if (arguments[1] != NULL) {
                if (chdir(arguments[1]) != 0) {
                    perror("cd");
                }
            } else {
                char *home = getenv("HOME");
                if (home != NULL) {
                    chdir(home);
                }
            }
        } else if (strcmp(arguments[0], "pwd") == 0) {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                perror("getcwd() error");
            }
        } else if (strcmp(arguments[0], "echo") == 0) {
            expand_variables(arguments, arg_count);

            for (i = 1; i < arg_count; i++) {
                printf("%s", arguments[i]);
                if (i < arg_count - 1) {
                    printf(" ");
                }
            }
            printf("\n");
        } else if (strcmp(arguments[0], "exit") == 0) {
            exit(0);
        } else if (strcmp(arguments[0], "env") == 0) {
            if (arguments[1] != NULL) {
                char *env_var = getenv(arguments[1]);
                if (env_var != NULL) {
                    printf("%s\n", env_var);
                }
            } else {
                char **env = environ;
                while (*env != NULL) {
                    printf("%s\n", *env);
                    env++;
                }
            }
        } else if (strcmp(arguments[0], "setenv") == 0) {
            if (arguments[1] != NULL) {
                char *equal_sign = strchr(arguments[1], '=');
                if (equal_sign != NULL) {
                    *equal_sign = '\0';
                    char *var_name = arguments[1];
                    char *var_value = equal_sign + 1;

                    if (setenv(var_name, var_value, 1) != 0) {
                        perror("setenv");
                    }
                } else if (arguments[2] != NULL) {
                    if (setenv(arguments[1], arguments[2], 1) != 0) {
                        perror("setenv");
                    }
                } else {
                    fprintf(stderr, "setenv: missing value for variable\n");
                }
            } else {
                fprintf(stderr, "setenv: missing arguments\n");
            }
        } else {
            // Fork a child process
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                exit(1);
            } else if (pid == 0) {
                // Child process
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGALRM, sigalrm_handler);
                alarm(10);

                // If input redirection is present, open the input file and redirect stdin
                if (redirect_input) {
                    int input_fd = open(input_file, O_RDONLY);
                    if (input_fd < 0) {
                        perror("open input file");
                        exit(1);
                    }
                    // Redirect input_fd to stdin (file descriptor 0)
                    if (dup2(input_fd, STDIN_FILENO) < 0) {
                        perror("dup2 input redirection");
                        exit(1);
                    }
                    close(input_fd); // Close the file descriptor, it is now duplicated to stdin
                }

                // If it's a background process, redirect errors to /dev/null
                if (background) {
                    int dev_null_fd = open("/dev/null", O_WRONLY);
                    if (dev_null_fd >= 0) {
                        dup2(dev_null_fd, STDERR_FILENO);  // Redirect stderr to /dev/null
                        close(dev_null_fd);
                    }
                }

                // Try to execute the command
                if (execvp(arguments[0], arguments) == -1) {
                    // Check if the command is in the current directory
                    if (errno == ENOENT) {
                        char cmd_with_path[MAX_COMMAND_LINE_LEN];
                        snprintf(cmd_with_path, sizeof(cmd_with_path), "./%s", arguments[0]);

                        execvp(cmd_with_path, arguments);  // Try to execute with ./ prefix
                    }

                    // If it still fails, print the error
                    perror("execvp() failed");
                    fprintf(stderr, "An error occurred.\n");
                    exit(1);
                }
            } else {
                // Parent process
                if (!background) {
                    int status;
                    waitpid(pid, &status, 0);  // Wait for the foreground process to finish
                } else {
                    // Background process: don't wait, just return to the prompt
                    printf("/home/codio> ");
                    fflush(stdout);
                }
            }
        }
    }
    return 0;
}
