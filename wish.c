#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

#define handle_error(msg)   \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

#define BUFSIZE 80

int logout = 0;

int main(int argc, char **argv)
{
    char **args = NULL;
    char *command;
    char *token;
    int runInBackground = 0;

    char *line = NULL;
    size_t len = 0;
    ssize_t lineSize = 0;

    char *path = NULL;

    while (!logout)
    {
        printf("wish> ");
        lineSize = getline(&line, &len, stdin); // stdin for interactive mode only
        line[lineSize - 1] = '\0';

        token = strtok(line, " ");
        command = malloc((strlen(token) + 1) * sizeof(char));
        if (command == NULL)
        {
            handle_error("malloc");
        }
        strcpy(command, token);

        int i = 0;
        int argCount = 0;
        while (token != NULL) // parse arguments
        {
            if (strcmp(token, "exit") == 0)
            { // built in exit for now
                free(command);
                printf("Goodbye\n");
                exit(EXIT_SUCCESS);
            }

            if (strcmp(token, "&") == 0)
            {
                runInBackground = 1;
                break;
            }
            runInBackground = 0;

            args = realloc(args, (argCount + 1) * sizeof(char *));
            if (args == NULL)
            {
                handle_error("realloc");
            }
            args[argCount] = malloc((strlen(token) + 1) * sizeof(char));
            if (args[argCount] == NULL)
            {
                handle_error("malloc");
            }
            strcpy(args[argCount], token);
            argCount++;
            i++;

            token = strtok(NULL, " ");
        }
        
        if (line != NULL) { // line no longer needed
            free(line);
            line = NULL;
        }

        args = realloc(args, (argCount + 1) * sizeof(char *));
        args[argCount] = NULL; // add execv's required NULL pointer to end of args

        char pipeBuffer[BUFSIZE];
        int pipefd[2];
        if (pipe(pipefd) == -1)
        {
            handle_error("pipe");
        }

        pid_t id1 = fork();
        if (id1 > 0)
        { // forked parent process
            close(pipefd[1]);
            if (runInBackground == 0)
            {
                wait(NULL);
                while (read(pipefd[0], pipeBuffer, sizeof(pipeBuffer)) > 0)
                    printf("%s\n", pipeBuffer);
                close(pipefd[0]);
            }
        }
        else
        { // forked child process
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            if (path != NULL) {
                free(path);
                path = NULL;
            }
            path = malloc((lineSize + 5) * sizeof(char));
            if (path == NULL)
            {
                handle_error("malloc");
            }
            strcpy(path, "/bin/");
            strcat(path, command);
            if (access(path, X_OK) == 0) {
                execv(path, args);
                handle_error("execv");
            }

            strcpy(path, "/usr/bin/");
            strcat(path, command);
            if (access(path, X_OK) == 0)
            {
                execv(path, args);
                handle_error("execv");
            }
            handle_error("command not found in binaries.");
        }

        // memory cleanup
        free(command);
        for (int i = 0; i < argCount; i++)
        {
            free(args[i]);
        }
        free(args);
        args = NULL;
    }

    printf("Goodbye\n");
    return 0;
}