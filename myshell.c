/**
 * header 
 * 
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include<readline/readline.h>
#include<readline/history.h>


void run(char *);
void run_pipe(char *, int, int);
int shell_cd(char *);
void redirect_control(char **);
char **tokenize(char *);

int stdoutcpy;


int main(int argc, char *argv[]) {
    stdoutcpy = dup(STDOUT_FILENO);
    int nbyte, homelen;
    char *inbuf, cwd[256], *home = getenv("HOME"), *user = getenv("USER"), host[_SC_HOST_NAME_MAX + 1], *temp;
    char **tokenized;
    gethostname(host, _SC_HOST_NAME_MAX + 1);
    homelen = strlen(home);

    while(1) {
        if(getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("\033[1;32m");   // Change color to green
            printf("%s@%s:", user, host);  // Print user name and host name
            printf("\033[1;34m");   // Change color to blue
            if(strncmp(cwd, home, homelen) == 0)
                printf("~%s", cwd+homelen);
            else
                printf("%s", cwd);
            printf("\033[0m");    // Change color back to deafult
            fflush(stdout);
        }
        else
            printf("cwd error\n");

        inbuf = readline("$ ");
        if(strlen(inbuf) != 0) {
            add_history(inbuf);
        }

        run(inbuf);
    }

    return 0;
}

void redirect_control(char **args) {
    int i=0;
    char *ptr, *files[256];
    while(args[i] != NULL) {
        if(strcmp(args[i], "2>&1") == 0) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
            args[i] = NULL;
        }
        else if(strcmp(args[i], "2>>") == 0) {
            int fd = open(args[i+1], O_WRONLY | O_CREAT, S_IRWXU);
            if(fd == -1) {
                perror("open");
                exit(0);
            }
            lseek(fd, 0, SEEK_END);
            dup2(fd, STDERR_FILENO);
            close(fd);
            args[i] = NULL;
        }
        else if(strcmp(args[i], "&>>") == 0) {
            int fd = open(args[i+1], O_WRONLY | O_CREAT, S_IRWXU);
            if(fd == -1) {
                perror("open");
                exit(0);
                args[i] = NULL;
            }
            lseek(fd, 0, SEEK_END);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
            args[i] = NULL;
        }
        else if(strcmp(args[i], ">>") == 0) {
            int fd = open(args[i+1], O_WRONLY | O_CREAT, S_IRWXU);
            if(fd == -1) {
                perror("open");
                exit(0);
            }
            lseek(fd, 0, SEEK_END);
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
        }
        else if(strcmp(args[i], "2>") == 0) {
            int fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            if(fd == -1) {
                perror("open");
                exit(0);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
            args[i] = NULL;
        }
        else if(strcmp(args[i], "&>") == 0 || strcmp(args[i], ">&") == 0) {
            int fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            if(fd == -1) {
                perror("open");
                exit(0);
            }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
            args[i] = NULL;
        }
        else if(strcmp(args[i], ">") == 0) {
            int fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            if(fd == -1) {
                perror("open");
                exit(0);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
        }
        else if(strcmp(args[i], "<") == 0) {
            int fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            if(fd == -1) {
                perror("open");
                exit(0);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
        }

        i++;
    }

    return;
}

// Runs the given command
void run(char *inbuf) {
        int i, pid;
        char **args = tokenize(inbuf);

        // Delete comment
        for(i=0; i<strlen(inbuf)+1; i++) {
            if(inbuf[i] == '#') {
                inbuf [i] = '\0';
                break;
            }
        }

        if(strncmp(inbuf, "exit", 4) == 0)
            exit(0);

        if(strncmp(inbuf, "cd", 2) == 0) {
            shell_cd(inbuf);
            return;
        }

        for(i = 0; i<strlen(inbuf)+1; i++) {
            if(inbuf[i] == '|') {
                run_pipe(inbuf, i, STDIN_FILENO);
                return;
            }
        }


        pid = fork();
        if(pid == 0) {  /* Child Process */
            redirect_control(args);

            if(args[0] == NULL || args[0][0] == '\0')
                return;

            execvp(args[0], args);
            perror("execvp");
            free(args);
            exit(1);
        }
        else if(pid > 0) {  /* Parent Process */
            wait(NULL); /* Wait for child process to terminate */
            free(args);
        }
        else {
            perror("fork");
            // exit(1);
        }
}

// Runs commands with pipes
void run_pipe(char *inbuf, int pos, int in_fd) {
    int i, pid, newpos = -1, pipe_fd[2];
    char *new_commands, *args[256];
    
    pipe(pipe_fd);

    if(pos != -1)
        new_commands = inbuf + pos + 1;
    else
        new_commands = NULL;

    if(pos != -1)
        inbuf[pos] = '\0';

    pid = fork();
    if(pid == 0) { /* Child Process */
        close(pipe_fd[0]);

        if(in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }

        if(new_commands != NULL) {
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[1]);
        }

        run(inbuf);
        exit(0);

    }
    else if (pid > 0) {
        close(pipe_fd[1]);

        for(i = 0; new_commands != NULL && i<strlen(new_commands)+1; i++) {
            if(new_commands[i] == '|') {
                newpos = i;
                break;
            }
        }

        wait(NULL);

        if(new_commands != NULL)
            run_pipe(new_commands, newpos, pipe_fd[0]);

        close(pipe_fd[0]);
    }
    else {
        perror("run_pipe fork");
        exit(0);
    }

}

// Changes directory
int shell_cd(char *inbuf) {
    char **args = tokenize(inbuf);
    int i=0;

    while (args[i] != NULL){
        i++;
    }
    if(i>2) {
        printf("bash: cd: too many arguments\n");
        return -1;
    }
    
    // Change directory to home if there is no argument
    if(args[1] == NULL) {
        if(chdir(getenv("HOME")) == -1) {
            perror("bash: cd:");
            return 1;
        }
    }
    else if (chdir(args[1]) == -1) {   // Change directory to given path
        printf("bash: cd: %s: %s\n", args[1], strerror(errno));
        return 1;
    }

    return -1;
}


char **tokenize(char *command) {
    int i, j, flag=0;
    /* String array */
    char **tokenized = (char **)malloc(64 * sizeof(char *));
    for(i=0; i<64; i++) {
        tokenized[i] = (char *)calloc(64, sizeof(char));
    }

    i=0; j=0;
    while (*command != '\0') {
        switch (*command) {
            case ' ':
            case '\t':
            case '\n':
            case '\v':
            case '\f':
            case '\r':
                if(flag == 0 && tokenized[i][0] != '\0') {
                    i++; j=0;
                }
                else if (flag == 1) {
                    tokenized[i][j] = *command;
                    j++;
                }
                break;

            case '\"':
            case '\'':
                if(flag == 0)
                    flag = 1;
                else
                    flag = 0;
                break;
            
            
            case '\\':
                if(*(command + 1) == ' ' || *(command + 1) == '\t' || *(command+1) == '\n') {
                    command++;
                    tokenized[i][j] = *command;
                    j++;
                }
                else {
                    tokenized[i][j] = *command;
                    j++;
                }
                break;

            default:
                tokenized[i][j] = *command;
                j++;
                break;
        }
        if(strcmp(tokenized[0], "ls") == 0 && strcmp(tokenized[1], "--color=auto") != 0) {
            strcpy(tokenized[1], "--color=auto");
            i++; j=0;
        }
        command++;
    }

    i=0;
    while(1) {
        if(tokenized[i][0] == '\0') {
            tokenized[i] = NULL;
            break;
        }
        i++;
    }

    return tokenized;
}
