#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glob.h>

#define PROMPT "mysh> "
#define MAX_ARGS 64
int executed = 0;
int batchMode = 0;

#define MAX_ARGS 64
struct Job
{
    char *name;  // empty
    char **args; // empty
    char *name2; // empty
    char **args2;
    char **tokenized;    // the entire token, after strtok
    int input_redirect;  // flag indicating input redirection
    char *input_file;    // file for input redirection
    int output_redirect; // flag indicating output redirection
    char *output_file;
    int hasPipe; // file for output redirection
    int hold;
};

#define MAX_PATH_LEN 256

void expand_wildcards(char **args) {
    glob_t results;
    int i, j;
    for (i = 0, j = 0; args[i] != NULL; i++) {
        if (strchr(args[i], '*') != NULL) {
            glob(args[i], GLOB_NOCHECK | GLOB_TILDE, NULL, &results);
            for (int k = 0; k < results.gl_pathc; k++) {
                args[j++] = strdup(results.gl_pathv[k]);
            }
            globfree(&results);
        } else {
            args[j++] = args[i];
        }
    }
    args[j] = NULL; // Null-terminate the modified array
}


void remove_redirection_and_pipe(char **args) {
    int i, j;

    for (i = 0, j = 0; args[i] != NULL; i++) {
        if (strchr(args[i], '>') == NULL && strchr(args[i], '<') == NULL && strchr(args[i], '|') == NULL) {
            args[j] = args[i];
            j++;
        }
    }

    args[j] = NULL;  // Null-terminate the modified array
}

// Function to search for a program in specified directories
char *search_program(char *program_name)
{
    // List of directories to search in
    char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin"};

    // Check if the program name contains a slash
    if (strchr(program_name, '/') == NULL)
    {
        // Iterate through directories
        for (int i = 0; i < sizeof(directories) / sizeof(directories[0]); i++)
        {
            // Build the full path
            char full_path[MAX_PATH_LEN];
            snprintf(full_path, sizeof(full_path), "%s/%s", directories[i], program_name);

            // Check if the file exists and is executable
            if (access(full_path, X_OK) == 0)
            {
                // File found, return the full path
                char *result = (char *)malloc(strlen(full_path) + 1);
                strcpy(result, full_path);
                return result;
            }
        }

        // If the loop completes, the program was not found
        fprintf(stderr, "Program '%s' not found in the specified directories.\n", program_name);
        return NULL;
    }
    else
    {
        // If the program name contains a slash, return the full path
        char *result = (char *)malloc(strlen(program_name) + 1);
        strcpy(result, program_name);
        return result;
    }
}


//Function to actually execute the job
void execution(struct Job *cmd) {
    int pipefd[2];
    int pid1, pid2;
    int inputfile = -1, outputfile = -1;
    int hasPipe = 0, hasOutputRedirect = 0, hasInputRedirect = 0;
    int outputRedirectIndex = -1, inputRedirectIndex = -1;
    //expand_wildcards(cmd -> args);

    // Check for pipe, output redirection, and input redirection
    for (int i = 0; cmd->tokenized[i] != NULL; i++) {
        if (strcmp(cmd->tokenized[i], "|") == 0) {
            hasPipe = 1;
            cmd->tokenized[i] = NULL;  // Split the command at the pipe symbol
        } else if (strcmp(cmd->tokenized[i], ">") == 0) {
            hasOutputRedirect = 1;
            outputRedirectIndex = i;
        } else if (strcmp(cmd->tokenized[i], "<") == 0) {
            hasInputRedirect = 1;
            inputRedirectIndex = i;
        }
    }

    if (hasPipe && pipe(pipefd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }



    remove_redirection_and_pipe(cmd -> args);

 
    // First command (potentially with redirections)
    pid1 = fork();
    if (pid1 == 0) {
        if (hasPipe) {
            close(pipefd[0]); // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
            close(pipefd[1]);
        }

        if (hasOutputRedirect) {
            outputfile = open(cmd->tokenized[outputRedirectIndex + 1], O_CREAT | O_TRUNC | O_WRONLY, 0640);
            if (outputfile < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(outputfile, STDOUT_FILENO); // Redirect stdout to file
            close(outputfile);
        }

        if (hasInputRedirect) {
            inputfile = open(cmd->tokenized[inputRedirectIndex + 1], O_RDONLY);
            if (inputfile < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(inputfile, STDIN_FILENO); // Redirect stdin from file
            close(inputfile);
        }

        char *path_to_command = search_program(cmd->name);
        execv(path_to_command, cmd->args);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    // Second command (if there's a pipe)
    if (hasPipe) {
        pid2 = fork();
        if (pid2 == 0) {
            close(pipefd[1]); // Close unused write end
            dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to read from pipe
            close(pipefd[0]);

            char *path_to_command = search_program(cmd->tokenized[outputRedirectIndex + 2]);
            execv(path_to_command, &cmd->tokenized[outputRedirectIndex + 2]);
            perror("execv");
            exit(EXIT_FAILURE);
        }
    }

    // Parent closes pipe and waits for children
    if (hasPipe) {
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
    } else {
        waitpid(pid1, NULL, 0);
    }
}



void build_and_execute(char *command)
{
    struct Job cmd;

    cmd.input_redirect = 0;
    cmd.output_redirect = 0;

    char *token;
    int argIndex = 0;

    cmd.args = (char **)malloc(sizeof(char *) * MAX_ARGS);
    cmd.tokenized = (char **)malloc(sizeof(char *) * MAX_ARGS);
    FILE *file;
    char *batch_job;

    //Checking for batch mode to know if we need to read the contents of the file
    if(batchMode){
        file = fopen(command, "r");
        if (file == NULL){
            printf("Not a valid file.\n");
            exit(EXIT_FAILURE);
        }
         // Read size of text from file
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        batch_job = (char *) malloc(file_size + 1);
         if (batch_job == NULL) {
            perror("Error allocating memory");
            fclose(file);
        }

        fread(batch_job, 1, file_size, file);
        batch_job[file_size] = '\0';
        fclose(file);
     }

    if (batchMode){
        token = strtok(batch_job, " \t\n");
    }
    else{
        token = strtok(command, " \t\n");
    }


    while (token != NULL && argIndex < MAX_ARGS - 1)
    {
        cmd.tokenized[argIndex] = token;
        token = strtok(NULL, " \t\n");
        argIndex++;
    }
    cmd.tokenized[argIndex] = NULL;  // Null-terminate the tokenized array

    // Check for empty command
    if (cmd.tokenized[0] == NULL)
    {
        fprintf(stderr, "Error: No command specified\n");
        return;
    }
    // Check for built-in commands
    if (strcmp(cmd.tokenized[0], "cd") == 0)
    {
        if (cmd.tokenized[1] == NULL)
        {
            fprintf(stderr, "Error: cd requires an argument\n");
        }
        else if (chdir(cmd.tokenized[1]) != 0)
        {
            perror("chdir");
        }
    }
    else if (strcmp(cmd.tokenized[0], "pwd") == 0)
    {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("%s\n", cwd);
        }
        else
        {
            perror("getcwd");
        }
    }
    else if (strcmp(cmd.tokenized[0], "which") == 0)
    {
        if (cmd.tokenized[1] == NULL)
        {
            fprintf(stderr, "Error: which requires an argument\n");
        }
        else
        {
            // Implement which logic here
            char* output = search_program(cmd.tokenized[1]); 
            printf("%s\n", output);
        }
    }
    else
    {
        // Handle other commands
        cmd.name = cmd.tokenized[0];
        memcpy(cmd.args, cmd.tokenized, sizeof(char *) * MAX_ARGS);
        execution(&cmd);
    }

    // Free allocated memory
    if(batchMode){
        free(batch_job);
    }

    free(cmd.args);
    free(cmd.tokenized);
}

void execute_interactive_mode() {
    char *job_to_execute;
    
    printf("%s", PROMPT);
    job_to_execute = (char *)malloc(sizeof(char) * 4096);
    
    while (1) {
        if (fgets(job_to_execute, 4096, stdin)) {
            // Remove the newline character from the input
            job_to_execute[strcspn(job_to_execute, "\n")] = 0;

            if (strcmp(job_to_execute, "exit") == 0) {
                printf("mysh: exiting\n");
                break;
            } else {
                build_and_execute(job_to_execute);
                printf("%s", PROMPT);
            }
        } else {
            // Handle EOF or read error
            printf("\nmysh: exiting\n");
            break;
        }
    }

    free(job_to_execute);
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        execute_interactive_mode();
    }
    else
    {
        batchMode = 1;
        build_and_execute(argv[1]);
    }

    return 0;
}
