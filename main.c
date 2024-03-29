#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

char str[1000];  //global variable to store the user input
char *args[20];  //global array to store the arguments
int argsCount;  //global variable to store number of arguments
int statusPtr;  //global variable to store the status of the child process

//functions prototypes
void onChildExit();
void log(pid_t id);
void setupEnvironment();
void shell();
void getInput();
void parseInput();
void evaluateArgs();
int  getType();
void executeCommand();
void executeShellBuiltin();
void cdCommand();
void echoCommand();
void exportCommand();
void closeFile();

int main() {
    signal(SIGCHLD,  onChildExit);
    setupEnvironment();
    shell();
    return 0;
}

//this function is called when a child process exits
void onChildExit() {
    pid_t childID;
    while ((childID = waitpid(-1, &childID, WNOHANG)) > 0) {
        printf("Child %d terminated with status %d\n", childID, statusPtr);
        log(childID);  //log to the file
    }
}

//this function logs the termination process to the log file
void log(pid_t id) {
    FILE *lg;  //pointer to file
    lg = fopen("log.txt", "a");  //open file for writing
    fprintf(lg, "Child process with id %d terminated\n", id);  //write text to file
    fclose(lg);  //close the file
}

//this function sets up the current working directory of the shell
void setupEnvironment() {
    chdir(getenv("PWD"));
}

//this function is the main loop of the shell, which reads input from the user, parses it, and executes it
void shell() {
    do {
        getInput();  //get the input from user
        parseInput();  //parse the input into command and arguments
        evaluateArgs();  //evaluate any environment variables in the arguments
        int inputType = getType();  //determine the type of input whether it is builtin or external command
        switch (inputType) {
            case 2:
                executeCommand();  //execute an external command
                break;
            case 1:
                executeShellBuiltin();  //execute a builtin command
                break;
            default:
                break;
        }
    } while (strcmp(args[0], "exit") != 0);  //continue looping until the user enters "exit" command
    closeFile();
}

//this function gets the input from the user
void getInput() {
    usleep(50000);  //sleep for better synchronization
    char cwd[1000];
    getcwd(cwd, 1000);
    printf("%s$ ", cwd);  //print the prompt
    fgets(str, 1000, stdin);  //read user input
    str[strcspn(str, "\n")] = 0;  //remove newline character from input
}

//this function parses the user input into arguments
void parseInput() {
    int i = 0;
    char *token = strtok(str, " ");
    while (token != NULL) {
        args[i++] = token;  //store each argument in the args array
        token = strtok(NULL, " ");
    }
    argsCount = i;  //store the number of arguments
    args[i] = NULL;
}

//evaluates each argument and replaces any environment variable references with their values
void evaluateArgs() {
    if(strcmp(args[0], "echo") == 0) {  //removing the double quotes for echo command
        args[1]++;
        args[argsCount - 1][strlen(args[argsCount - 1]) - 1] = '\0';
    }
    char newStr[1000];  //create a new string to store the evaluated arguments
    memset(newStr, 0, 1000);  //clear the new string
    for (int i = 0; args[i] != NULL; i++) {  //loop through each argument
        if(args[i][0] == '$') {  //if the argument starts with a dollar sign
            char *variable = & args[i][1];  //get the variable name
            sprintf(newStr, "%s %s",newStr, getenv(variable));  //concatenate the evaluated value of the environment variable to the new string
        } else {
            sprintf(newStr, "%s %s", newStr, args[i]);  //concatenate the argument to the new string
        }
    }
    strcpy(str, newStr);  //copy the new string to the original string
    parseInput();  //parse the input again to set the new arguments
}

//this function determines the type of input whether it is builtin command or external command
int getType() {
    if (strcmp(args[0], "cd") == 0 || strcmp(args[0], "echo") == 0 || strcmp(args[0], "export") == 0)
        return 1;
    else if (strcmp(args[0], "exit") == 0)
        return 0;
    else
        return 2;
}

//this function executes the external commands
void executeCommand() {
    char *lastCommand;
    int i = 0;
    while(args[i] != NULL)
        lastCommand = args[i++];  //get the last argument to check for waiting the child or not ($)
    pid_t childID = fork();  //fork a child process
    if (childID == 0) {
        execvp(args[0], args);  //execute the command in the child process
        perror("execvp");  //print an error message if the command failed to execute
        exit(1);  //exit the child process with an error code
    } else if (childID < 0) {
        perror("Error while forking");  //print an error message if forking failed
    } else if(strcmp(lastCommand, "&") != 0) {
        waitpid(childID, &statusPtr, 0);  // ait for the child process
    }
}

//this function executes the builtin shell commands
void executeShellBuiltin() {
    char *cmd = args[0];
    if (strcmp(cmd, "cd") == 0) {
        cdCommand();
    } else if (strcmp(cmd, "echo") == 0) {
        echoCommand();
    } else if (strcmp(cmd, "export") == 0) {
        exportCommand();
    }
}

//changes the current working directory to the given path or to the home directory if no path is given or the path is "~"
void cdCommand() {
    char *path = args[1];  //gets the argument at index 1 (path)
    if (path == NULL || strcmp(path, "~") == 0) {  //if path is null or "~"
        chdir(getenv("HOME"));  //change the directory to the HOME environment variable
    } else {
        chdir(path);  //change the directory to the given path
    }
}

//prints out the arguments passed to the echo command
void echoCommand() {
    for (int i = 1; args[i] != NULL; i++)  //loop through each argument
        printf("%s ", args[i]);  //print the argument
    printf("\n");  //print a newline character at the end
}

//exports the given variables to the environment variables list
void exportCommand() {
    for (int i = 1; args[i] != NULL; i++) {  //loop through each argument
        char *variable = strtok(args[i], "="), *value = strtok(NULL, "=");  //get each variable name and its value
        if(value[0] == '"') {  //if value starts with a quotation mark
            args[i] = value;
            value++;
            while(args[i][strlen(args[i]) - 1] != '"') {  //while the current argument doesn't end with a quotation mark
                i++;  //increment the index
                if(args[i] == NULL) {  //if there are no more arguments
                    perror("error in arguments");  //print an error message
                }
                sprintf(value, "%s %s", value, args[i]);  //concatenate the value with the next argument
            }
            args[i][strlen(args[i]) - 1] = '\0';  //remove the ending quotation mark from the last argument
        }
        setenv(variable, value, 1);  //set the environment variable with the given name and value
    }
}

//this function prints line when closing the shell to separate between different runs
void closeFile() {
    FILE *lg;  //pointer to file
    char text[101];  //store the line to be printed in the file
    for(int i = 0; i < 100; i++)
        text[i] = '=';
    text[100] = '\0';
    lg = fopen("log.txt", "a");  //open file for writing
    fprintf(lg, "%s\n", text);  //write text to file
    fclose(lg);  //close the file
}