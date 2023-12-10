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

int main(int argc, char **argv)
{
    char **args = NULL;
    char *command;
    char *token;
    int runInBackground = 0;
    int error = 0;
    int exit_flag = 0;

    char *line = NULL;
    size_t len = 0;
    ssize_t lineSize = 0;

    char * init_paths[] = {"/bin","/usr/bin"};
    int num_paths = 2;
    char **paths = NULL;
    char *path = NULL;
    int path_found = 0;

    paths = malloc((num_paths) * sizeof(char *));
    if (paths == NULL)
    {
        handle_error("malloc");
    }
    
    for (int i = 0; i < num_paths; i++) {
        paths[i] = init_paths[i];
    }       

    while (!exit_flag)
    {
        error = 0;
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
            if (strcmp(command, "exit") == 0)
            { // built in exit command
                token = strtok(NULL, " ");
                if (token != NULL) {
                    printf("usage: exit\n");
                    error = 1;
                    break;
                }
                exit_flag = 1;
                break;
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

        if (!exit_flag && !error) {
            args = realloc(args, (argCount + 1) * sizeof(char *));
            if (args == NULL)
            {
                handle_error("realloc");
            }
            args[argCount] = NULL; // add execv's required NULL pointer to end of args
            
            char pipeBuffer[BUFSIZE] = "";
            int pipefd[2];
            if (pipe(pipefd) == -1)
            {
                handle_error("pipe");
            }
            
            // set arbitrary number of new paths, overwrite old paths
            if (strcmp(command, "path") == 0)
            { // built in path command
                if (argCount == 1) { // no paths
                    free(paths);
                    paths = NULL;
                    num_paths = 0;
                    continue;
                }
                // reallocate memory in paths
                paths = realloc(paths, (argCount - 1) * sizeof(char *));
                num_paths = argCount - 1;
                if (paths == NULL)
                {
                    handle_error("realloc");
                }

                // re-populate paths
                for (int i = 0; i < num_paths; i++) {
                    paths[i] = args[i+1];
                    printf("path: %s.\n", paths[i]);
                }   
                continue;
            }

            // check for bin
            // store path for forked child to execv into
            path_found = 0;
            for (int i = 0; i < num_paths; i++) {
                path = malloc((strlen(paths[i]) + strlen(command) + 2) * sizeof(char));
                if (path == NULL)
                {
                    handle_error("malloc");
                }
                strcpy(path, paths[i]);
                strcat(path, "/");
                strcat(path, command);
                if (access(path, X_OK) == 0) {
                    path_found = 1;
                    break;
                }
            }   
            if (!path_found) {
                perror("bin not found in paths");
                continue;
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
            else if (id1 == 0)
            { // forked child process
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);

                execv(path, args);
                handle_error("execv");
            }
        }

        // memory cleanup
        if (command != NULL) { // line no longer needed
            free(command);
            command = NULL;
        }

        for (int i = 0; i < argCount; i++)
        {
            if (args[i] != NULL) { // line no longer needed
                free(args[i]);
                args[i] = NULL;
            }
        }
        if (args != NULL) { // line no longer needed
            free(args);
            args = NULL;
        }

        if (path != NULL) { // line no longer needed
            free(path);
            path = NULL;
        }
    }

    for (int i = 0; i < num_paths; i++)
    {
        if (paths[i] != NULL) {
            free(paths[i]);
            paths[i] = NULL;
        }
    }
    if (paths != NULL) {
        free(paths);
        paths = NULL;
    }
    printf("Goodbye\n");
    return 0;
}