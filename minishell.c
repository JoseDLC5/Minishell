// I pledge my honor that I have abided by the Stevens Honor System.
// Jose de la Cruz
// Breanna Shinn

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>
#include <setjmp.h>

#define BRIGHTBLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m"

volatile bool interrupted = false;
char currentDirectory[PATH_MAX];
int argumentCounter;
pid_t pid;
sigjmp_buf jmpbuf;
volatile sig_atomic_t childRunning;

int cd(int argc, char **arguments)
{
    char newDir[PATH_MAX];

    if (strcmp(arguments[1], "~") == 0)
    {
        uid_t uid = getuid();
        struct passwd *p = getpwuid(uid);
        if (p == NULL)
        {
            printf("Error: Cannot get passwd entry. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }

        strcpy(newDir, p->pw_dir);
        if (chdir(newDir) == -1)
        {
            printf("Error: Cannot change directory to '%s'. %s.\n", arguments[1], strerror(errno));
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    if (argc > 2)
    {
        printf("Error: Too many arguments to cd.\n");
        return EXIT_FAILURE;
    }

    strcpy(newDir, currentDirectory);
    strcat(newDir, "/");
    strcat(newDir, arguments[1]);

    if (chdir(newDir) == -1)
    {
        printf("Cannot change directory to '%s'. %s.\n", arguments[1], strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void catch_signal(int sig)
{
    interrupted = true;
    if (pid != 0)
    {
        siglongjmp(jmpbuf, 1);
    }
}

void fillArguments(char *line, char **arguments)
{
    arguments[0] = line;
    int index = 1;
    size_t lineLen = strlen(line);
    for (size_t i = 0; i < lineLen; i++)
    {
        if (*line == ' ' || *line == '\n')
        {
            *line = '\0';
            if (*(line + 1) != ' ')
            {
                arguments[index] = line + 1;
                index++;
            }
        }
        line++;
    }
    argumentCounter = index;
    arguments[index + 1] = '\0';
}

int runCommand(char **arguments)
{
    if (strlen(arguments[0]) == 0)
    {
        return EXIT_SUCCESS;
    }

    int waitRet;
    int status = 0;
    childRunning = 1;
    if ((pid = fork()) < 0)
    {
        printf("Error: fork() failed. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    else if (pid == 0)
    {
        if (execvp(arguments[0], arguments) < 0)
        {
            printf("Error: exec() failed. %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    else
    {
        waitRet = wait(&status);
        if (waitRet < 0)
        {
            // if (interrupted)
            // {
            //     return EXIT_FAILURE;
            // }
            printf("Error: wait() failed. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        else
        {
            return EXIT_SUCCESS;
        }
    }
}

int main()
{
    struct sigaction action;

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = catch_signal;

    if (sigaction(SIGINT, &action, NULL) == -1)
    {
        printf("Error: Cannot register signal handler. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    char line[2048];
    int bytesRead;
    char *arguments[128];
    sigsetjmp(jmpbuf, 1);
    while (1)
    {
        if (childRunning == 1)
        {
            wait(NULL);
        }
        childRunning = 0;

        memset(arguments, 0, sizeof arguments);
        memset(line, 0, sizeof line);
        if (interrupted)
        {
            printf("\n");
            interrupted = false;
            continue;
        }
        if (getcwd(currentDirectory, PATH_MAX) == NULL)
        {
            printf("Error: Cannot get current working directory. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        printf("%s[", DEFAULT);
        printf("%s%s", BRIGHTBLUE, currentDirectory);
        printf("%s]$ ", DEFAULT);
        fflush(stdout);
        if ((bytesRead = read(STDIN_FILENO, line, 1024)) < 0)
        {
            if (interrupted)
            {
                printf("\n");
                interrupted = false;
                continue;
            }
            printf("Error: Failed to read from stdin. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        line[bytesRead - 1] = '\0';
        fillArguments(line, arguments);

        if (strcmp(line, "exit") == 0)
        {
            return EXIT_SUCCESS;
        }
        else if (strcmp(arguments[0], "cd") == 0)
        {
            cd(argumentCounter, arguments);
        }
        else
        {
            runCommand(arguments);
        }
    }
}