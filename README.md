# **Custom Shell - Quash**

### **Author**: Fiyinfoluwa Afolayan

---

## **Introduction**

### **Objective**

The objective of this project is to understand the workings of a command-line interface (CLI) and to gain practical knowledge of process forking and signaling by building a custom shell in C. This shell, named **Quash**, is a simplified version of common command shells like `sh`, `bash`, `csh`, and `tcsh`.

### **Overview**

The shell was developed incrementally, implementing various features in stages:

1. **Dynamic Shell Prompt and Built-in Commands**
2. **External Command Execution with Process Forking**
3. **Background Process Handling**
4. **Signal Handling for Process Termination**
5. **Managing Long-Running Processes with Timers**
6. **Input/Output Redirection and Piping**

---

## **Design Choices and Implementation**

### **1. Dynamic Shell Prompt and Built-in Commands**

#### **Shell Prompt**

The shell prompt dynamically displays the current working directory, similar to standard shells. This is done using the `getcwd()` function.

```c
char cwd[PATH_MAX];
if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s> ", cwd);
} else {
    perror("getcwd() error");
    exit(1);
}
```

#### **Tokenization**

Input commands are tokenized using `strtok()` to parse commands and arguments effectively.

```c
char *token = strtok(command_line, delimiters);
while (token != NULL && arg_count < MAX_COMMAND_LINE_ARGS - 1) {
    arguments[arg_count++] = token;
    token = strtok(NULL, delimiters);
}
arguments[arg_count] = NULL;

```

#### **Tokenization**

- `cd`: Changes the current working directory using `chdir()`.
- `pwd`: Prints the current working directory.
- `echo`: Prints messages and supports environment variable expansion.
- `exit`: Exits the shell.
- `env`: Displays environment variables.
- `setenv`: Sets an environment variable.

### **2. External Command Execution with Process Forking**

To execute external commands, the shell forks a child process and replaces its image with the desired program using `execvp()`. The parent process waits for the child to complete unless it's a background process.

```c
pid_t pid = fork();
if (pid < 0) {
    perror("fork");
    continue;
} else if (pid == 0) {
    // Child process
    if (execvp(arguments[0], arguments) == -1) {
        perror("execvp() failed");
        exit(1);
    }
} else {
    // Parent process
    int status;
    waitpid(pid, &status, 0);
}
```

### **3. Background Process Handling**

#### **Design**

Commands appended with `&` are executed as background processes. The shell does not wait for background processes to finish and immediately returns to the prompt.

#### **Implementation**

- **Detecting Background Processes**: Checked for `&` in the tokenized input.

```c
if (strcmp(token, "&") == 0) {
    background = 1;
}
```

- **Parent Process Handling**: Modified the parent process logic to skip `waitpid()` for background processes.

```c
if (!background) {
    int status;
    waitpid(pid, &status, 0);
} else {
    printf("Process running in background with PID %d\n", pid);
}
```

### **4. Signal Handling for Process Termination**

#### **Design**

The shell should not terminate when the user presses `Ctrl-C` (SIGINT). Foreground child processes should be terminable via `Ctrl-C`.

#### **Implementation**

- **Signal Handler in Shell**: The shell process ignores `SIGINT`.

```c
void sigint_handler(int sig) {
    // Ignore SIGINT in the shell process
    printf("\n");
}

signal(SIGINT, sigint_handler);
```

- **Resetting Signal Handling in Child Process**: The child process resets `SIGINT` handling to default.

```c
if (pid == 0) {
    // Child process
    signal(SIGINT, SIG_DFL);
    // ...
}
```

### **5. Managing Long-Running Processes with Timers**

#### **Design**

Foreground processes running longer than 10 seconds are automatically terminated. Used `alarm()` and `SIGALRM` to implement timing functionality.

#### **Implementation**

- Signal Handler for `SIGALRM`:

```c
void sigalrm_handler(int sig) {
    printf("Process exceeded time limit and was killed.\n");
    exit(1);
}
```

- **Setting Up Alarm in Child Process**:

```c
if (pid == 0) {
    // Child process
    signal(SIGALRM, sigalrm_handler);
    alarm(10);
    // ...
}
```

### **6. Input/Output Redirection and Piping**

#### **Design**

Implemented output redirection (`>`), input redirection (`<`), and piping (`|`). The shell parses the command line to detect these operators and sets up file descriptors accordingly.

#### **Implementation**

**Output Redirection (`>`)**

- **Tokenization Logic**:

```c
if (strcmp(token, ">") == 0) {
    redirect_output = 1;
    token = strtok(NULL, delimiters);
    output_file = token;
}
```

- **Child Process Redirection**:

```c
if (redirect_output) {
    int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        perror("open output file");
        exit(1);
    }
    dup2(fd_out, STDOUT_FILENO);
    close(fd_out);
}
```

**Input Redirection (`<`)**

- **Tokenization Logic**:

```c
if (strcmp(token, "<") == 0) {
    redirect_input = 1;
    token = strtok(NULL, delimiters);
    input_file = token;
}
```

- **Child Process Redirection**:

```c
if (redirect_input) {
    int fd_in = open(input_file, O_RDONLY);
    if (fd_in < 0) {
        perror("open input file");
        exit(1);
    }
    dup2(fd_in, STDIN_FILENO);
    close(fd_in);
}

```

**Piping (`|`)**

- **Tokenization Logic**:

```c
if (strcmp(token, "|") == 0) {
    pipe_present = 1;
    arguments[arg_count] = NULL;
    // Collect right side of the pipe into pipe_args[]
}

```

- **Child Process Redirection**:

```c
if (pipe_present) {
    int pipe_fd[2];
    pipe(pipe_fd);
    pid_t pid2 = fork();
    if (pid2 == 0) {
        // First child process (left side)
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]);
        execvp(arguments[0], arguments);
    } else {
        // Second child process (right side)
        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[1]);
        execvp(pipe_args[0], pipe_args);
    }
}
```

### Testing and Examples

#### Built-in Commands

```c
/home/user> pwd
/home/user

/home/user> cd Documents
/home/user/Documents> pwd
/home/user/Documents

/home/user/Documents> echo Hello World
Hello World

/home/user/Documents> setenv MYVAR test
/home/user/Documents> echo $MYVAR
test
```

#### External Commands and Background Processes

```c
/home/user> ls -l
total 0
-rw-r--r-- 1 user user 0 Oct 1 12:00 file.txt

/home/user> sleep 20 &
Process running in background with PID 12345
/home/user>
```

#### Signal Handling

```c
/home/user> sleep 30
# Press Ctrl-C
/home/user>
```

#### Long-Running Process Termination

```c
/home/user> sleep 15
Process exceeded time limit and was killed.
/home/user>
```

#### Input/Output Redirection and Piping

```c
/home/user> echo "This is a test." > output.txt
/home/user> cat output.txt
This is a test.

/home/user> sort < unsorted.txt > sorted.txt

/home/user> cat unsorted.txt | grep "pattern"
```

### Challenges and Solutions

#### Parsing Complexity

- **Challenge**: Managing the parsing of commands with multiple special characters (e.g., `&, >, <, |`).
- **Solution**: Carefully structured the tokenization logic to detect and handle special operators, ensuring correct setup of variables for redirection and background processing.

#### Signal Handling Nuances
- **Challenge**: Ensuring that the shell remains responsive to `Ctrl-C` without terminating, while allowing child processes to be interrupted.
- **Solution**: Set the shell's `SIGINT` handler to ignore the signal and reset it to default in child processes.

#### Process Synchronization
- **Challenge**: Managing background processes and ensuring they don't interfere with the shell's operation.
- **Solution**: Implemented non-blocking waits for background processes and provided feedback to the user with process IDs

### Conclusion

Developing this custom shell provided valuable insights into:
- **Process Management**: Understanding how to create and manage child processes using `fork()` and `execvp()`.
- **Signal Handling**: Learning how signals like `SIGINT` and `SIGALRM` can control process behavior.
- **Input/Output Redirection**: Implementing redirection and piping to manipulate file descriptors.
- **Shell Functionality**: Gaining appreciation for the complexity involved in building a functional shell.
