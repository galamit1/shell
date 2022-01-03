#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char** arglist);

// prepare and finalize calls for initialization and destruction of anything required
int prepare(void);
int finalize(void);

int run_command(char** arglist);
int run_command_in_background(char** arglist);
int run_pipe(char** arglist, int pipe_index);
int run_output_redirection_command(char** arglist, int index);



int prepare(void) {
    return 0;
}


int run_command(char** arglist) {
    int status = 0;
    int pid = fork();
    if (pid == -1) {
        perror("Error in fork");
        return 0;
    }

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        if (execvp(arglist[0], arglist) == -1) {
            perror("Error");
            exit(1);
        }
    } else {
        waitpid(pid, &status, WUNTRACED);
    }
    return 1;
}

int run_command_in_background(char** arglist) {
    int status;
    int pid = fork();
    if (pid == -1) {
        perror("Error in fork");
        return 0;
    }
    if (pid == 0) {
        signal(SIGINT, SIG_IGN);
        if (execvp(arglist[0], arglist) == -1) {
            perror("Error");
            exit(1);
        }
    }
    waitpid(pid, &status, WNOHANG); //return immediately
    return 1;
}

int run_pipe(char** arglist, int pipe_index) {
    int status;
    int status2;
    int fd[2];
    if (pipe(fd) == -1) {
        perror("Error in pipe");
        return 0;
    }
    arglist[pipe_index] = NULL;

    int pid = fork();
    if (pid == -1) {
        perror("Error in fork");
        return 0;
    }

    if (pid == 0) {
            close(fd[0]);
            dup2(fd[1], STDOUT_FILENO);
            signal(SIGINT, SIG_DFL);
            if (execvp(arglist[0], arglist) == -1) {
                perror("Error");
                exit(1);
            }
            close(fd[1]);
    }
    else {
        int pid2 = fork();
        if (pid2 == -1) {
            perror("Error in fork");
            return 0;
        }
        if (pid2 == 0) { // second command
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            signal(SIGINT, SIG_DFL);
            if (execvp(arglist[pipe_index + 1], arglist + pipe_index + 1) == -1) { // the second command after the "|"
                perror("Error");
                exit(1);
            }
            close(fd[0]);
        }
        close(fd[0]);
        close(fd[1]);
        waitpid(pid, &status, WUNTRACED);
        waitpid(pid2, &status2, WUNTRACED);
    }
    return 1;
}

int run_output_redirection_command(char** arglist, int index) {
    int status;
    int fd = open(arglist[index + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd == -1) {
        perror("Error in opening file");
        return 0;
    }
    arglist[index] = NULL; // so the command will not run the > ...
    int pid = fork();
    if (pid == -1) {
        perror("Error in fork");
        return 0;
    }

    if (pid == 0) {
        dup2(fd, STDOUT_FILENO);
        signal(SIGINT, SIG_DFL);
        if (execvp(arglist[0], arglist) == -1) { // the second command after the "|"
            perror("Error");
            exit(1);
        }
        close(fd);
    }
    else {
        close(fd);
        waitpid(pid, &status, WUNTRACED);
    }
    return 1;
}


int process_arglist(int count, char** arglist) {
    int status;
    waitpid(-1, &status, WNOHANG);
    signal(SIGINT, SIG_IGN);
    if (strcmp(arglist[count - 1], "&") == 0) { // run in the background
        arglist[count - 1] = NULL;
        return run_command_in_background(arglist); // the arglist without the &
    }

    for (int i = 0; i < count; ++i) {
        if (strcmp(arglist[i], "|") == 0) { // pipe command
            return run_pipe(arglist, i);
        }
        if (strcmp(arglist[i], ">") == 0) { // output redirection
            return run_output_redirection_command(arglist, i);
        }
    }
    run_command(arglist);
    return 1;
}


int finalize() {
    return 0;
}