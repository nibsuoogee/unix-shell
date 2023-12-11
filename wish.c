#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <dirent.h> 

#define handle_error(msg)   \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

#define BUFSIZE 1024
#define PATH_MAX 4096

char **free_paths(char **paths, char *init_paths[], int num_paths);
void *null_check_free(void *ptr);

int main(int argc, char **argv)
{
    int exit_flag = 0;
    char **args = NULL;
    char *command = NULL;
    char *token = NULL;
    
    int runInBackground = 0;
    int redirect = 0;
    char *redirect_file;
    int redirect_fd;

    int argCount = 0;
    char *line = NULL;
    size_t len = 0;
    ssize_t lineSize = 0;

    char * init_paths[] = {"/bin","/usr/bin"};
    int num_paths = 2;
    char **paths = NULL;
    char *path = NULL;
    int path_found = 0;
    char cwd[PATH_MAX];

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
        // command cleanup
        line = null_check_free(line);
        command = null_check_free(command);
        for (int i = 0; i < argCount; i++)
        {
            args[i] = null_check_free(args[i]);
        }
        args = null_check_free(args);
        argCount = 0;
        path = null_check_free(path);
        redirect_file = null_check_free(redirect_file);

        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            handle_error("getcwd()");
        }
        printf("%s wish> ", cwd);
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
        argCount = 0;
        while (token != NULL) // parse arguments
        {
            if (strcmp(token, "&") == 0)
            {
                runInBackground = 1;
                break;
            }
            runInBackground = 0;

            if (strcmp(token, ">") == 0) {
                token = strtok(NULL, " ");
                if (token == NULL) {
                    fprintf(stderr, "usage: <command> <args> > <file> \n");
                }
                redirect_file = malloc((strlen(token) + 1) * sizeof(char));
                if (redirect_file == NULL)
                {
                    handle_error("malloc");
                }
                strcpy(redirect_file, token);
                token = strtok(NULL, " ");
                if (token != NULL) {
                    fprintf(stderr, "usage: command <args> > <file> \n");
                } else {
                    redirect = 1;
                }
                break;
            }
            
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

        if (strcmp(command, "exit") == 0)
        { // built in exit command
            if (argCount > 1) {
                printf("usage: exit\n");
                continue;
            } else {
                exit_flag = 1;
                continue;
            }
        }

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
            paths = free_paths(paths, init_paths, num_paths);
            num_paths = argCount - 1;
            if (num_paths == 0) {
                continue;
            }
            // reallocate memory in paths
            paths = malloc((num_paths) * sizeof(char *));
            if (paths == NULL)
            {
                handle_error("realloc");
            }

            for (int i = 0; i < num_paths; i++) {
                paths[i] = malloc((strlen(args[i+1]) + 1) * sizeof(char));
                if (paths[i] == NULL)
                {
                    handle_error("malloc");
                }
            }

            // re-populate paths
            for (int i = 0; i < num_paths; i++) {
                strcpy(paths[i], args[i+1]);
            }
            continue;
        }

        if (strcmp(command, "cd") == 0)
        { // built in cd command           
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                handle_error("getcwd()");
            }
            if (argCount - 1 < 1 || argCount - 1 > 1 ) {
                fprintf(stderr, "usage: cd <dir> \n");
                continue;
            }
            
            DIR *d;
            struct dirent *dir;
            d = opendir(cwd);
            if (d) {
                while ((dir = readdir(d)) != NULL) {
                    if (strcmp(args[1], dir->d_name) == 0) {
                        strcat(cwd, "/");
                        strcat(cwd, dir->d_name);
                        if (chdir(cwd) != 0) {
                            handle_error("chdir()");
                        };
                    }
                }
                closedir(d);
            }
            continue;
        }

        // check for bin
        path_found = 0;
        for (int i = 0; i < num_paths; i++) {
            path = null_check_free(path);
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
            fprintf(stderr, "bin not found in path\n");
            continue;
        }

        pid_t id1 = fork();
        if (id1 > 0)
        { // forked parent process
            close(pipefd[1]);
            // > redirection here
            int output_fd = 1;
            if (redirect) {
                redirect_fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                output_fd = redirect_fd;
            }

            if (redirect == 1 || runInBackground == 0)
            {
                wait(NULL);
                ssize_t bytesRead;
                while ((bytesRead = read(pipefd[0], pipeBuffer, sizeof(pipeBuffer))) > 0) {
                    if (write(output_fd, pipeBuffer, bytesRead) != bytesRead) {
                        handle_error("write");
                    }
                }
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

    // shell exit cleanup 
    line = null_check_free(line);
    command = null_check_free(command);
    for (int i = 0; i < argCount; i++)
    {
        args[i] = null_check_free(args[i]);
    }
    args = null_check_free(args);
    argCount = 0;
    path = null_check_free(path);
    redirect_file = null_check_free(redirect_file);
    
    paths = free_paths(paths, init_paths, num_paths);

    printf("Goodbye\n");
    return 0;
}

char **free_paths(char **paths, char *init_paths[], int num_paths) {
    for (int i = 0; i < num_paths; i++)
    {
        if (paths[i] != NULL) {
            // free if not pointing to original stack allocated paths
            if (paths[i] != init_paths[i]) {
                free(paths[i]);
                paths[i] = NULL;
            }
        }
    }
    if (paths != NULL) {
        free(paths);
        paths = NULL;
    }
    return paths;
}

void *null_check_free(void *ptr) {
    if (ptr != NULL) {
        free(ptr);
        ptr = NULL;
    }
    return ptr;
}