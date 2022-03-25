/**
 *Program Name: yash.h
 *Author : Ashma Parveen
 *This program reads the input from the client's terminal and executes commands by creating processes as a shell.
 *It implements a subset of features supported by a standard shell like bash/zsh.
 **/
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>

#define fg 1
#define bg 0
#define BUFSIZE 1024

/* Declaration of strcutures*/

typedef enum
{
    RUNNING,
    STOPPED
} JobStatus;

struct processList
{
    char *processString;
    int groupId;
    pid_t cpid;
    char *inputPath;
    char *outputPath;
    char **processArgs;
    struct processList *next;
};

struct jobList
{
    int jobId;
    char *jobCommand;
    int jobCount;
    char *jobSign;
    JobStatus jobStatus;
    struct processList *process;
    int terminal;
    struct jobList *next;
};

struct ClientJobList
{
    int terminalID;
    int clientJobNumber;
    struct jobList *job;
    struct ClientJobList *next;
};

typedef struct waitStruct
{
    int psd;
    int groupId;
    int doneFlag;
    int outfd;
    int inputEnd;
    struct sockaddr_in cliAddr;
} waitStruct;

/* Declaration of Functions*/
void printZombies();
int *getPidListClient(struct ClientJobList *headRef);
void *waitingThread(void *param);
void checkForCTLcmd(char *cmd, int pid, int *doneFlag, int outfd, int psd, struct sockaddr_in cliAddr, int rc, int inputEnd);
struct jobList *deleteJobByPid(struct jobList **headRef, int pid, int psd);
void deleteByPIDClientList(struct ClientJobList **rootClientJobList, int pid, int psd);
int *getPidList(struct jobList **headRef);
const char *getJobsStatus(JobStatus jobStatus);
struct ClientJobList *search(struct ClientJobList **headRef, int terminalID);
void printdone(struct jobList *root);
struct jobList *newNode(int jobId, char *jobCommand, JobStatus jobStatus, int jobCount, struct processList *process, int terminal);
void pushJob(struct jobList **root, int jobId, char *jobCommand, JobStatus jobStatus, int jobCount, struct processList *process, int terminal);
struct jobList *popJob(struct jobList **root);
void printJobs(struct jobList *root, int psd);
struct jobList *deleteJobByStatus(struct jobList **headRef, JobStatus jobStatus);
int checkIfShellCommands(char *inString);
char **parseStringStrtok(char *str, char *delim);
struct processList *parseSubCommand(char **subCommand);
struct processList *parseStringforPipes(char **parsedCmdArray);
int exexuteCommands(struct processList *rootProcess, int infd, int outfd, int job_type, int psd, struct jobList **rootJob, struct sockaddr_in cliaddr);
int executeParsedCommand(struct processList *rootProcess, int job_type, int psd, struct jobList **rootJob, struct sockaddr_in cliaddr);
void executeShellCommands(char *inString, int psd, struct jobList **rootJob, struct sockaddr_in cliaddr);
char **parsecommands(char *inString, int psd, struct jobList **rootJob, struct sockaddr_in cliaddr);
void sigChildHandler(int signo);
int isEmpty(struct jobList *root);
void checkPrevJobCount(struct jobList **temp, int psd);
void assigbJobSign(struct jobList **jobRef);
int getProcessCount();
void waitFunction(int pid, int psd, int *waitStatus, struct sockaddr_in cliaddr, int outfd, int inputEnd);

pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t childTlock = PTHREAD_MUTEX_INITIALIZER;

struct ClientJobList *rootClientJobList = NULL;

/** This function checks for empty list**/
int isEmpty(struct jobList *root)
{
    return !root;
}
int isEmptyClient(struct ClientJobList *root)
{
    return !root;
}

void printZombies(int psd)
{
    int *pidlist;
    struct ClientJobList *clientList = NULL;
    pidlist = getPidListClient(rootClientJobList);

    if (pidlist != NULL)
    {
        int arraySize = sizeof(pidlist);
        int intSize = sizeof(pidlist[0]);
        int length = arraySize / intSize;
        int pidsize = 0;
        int index = 0;
        while (index < length)
        {

            int status, pid;
            if (pidlist[index] != 0)
            {

                pid = waitpid(pidlist[index], &status, WNOHANG);
                struct jobList *jobObj =
                    (struct jobList *)
                        malloc(sizeof(struct jobList));
                deleteByPIDClientList(&rootClientJobList, pid, psd);
            }
            index++;
        }
    }
}
void cleanup(char *buf)
{
    int i;
    for (i = 0; i < BUFSIZE; i++)
        buf[i] = '\0';
}

/** This function is to search the job in the joblist and returns true if present**/
struct ClientJobList *search(struct ClientJobList **headRef, int terminalID)
{

    struct ClientJobList *temp = *headRef;

    if (isEmptyClient(*headRef))
    {
        return NULL;
    }
    while (temp->terminalID != terminalID)
    {
        if (temp->next == NULL)
        {
            return NULL;
        }
        else
        {

            temp = temp->next;
        }
    }
    return temp;
}
int *getPidListClient(struct ClientJobList *headRef)
{

    struct ClientJobList *clientListTemp = headRef;
    struct jobList *jobTemp = NULL;
    int *pidlist = (int *)malloc(sizeof(int) * 1000);
    int index = 0;
    if (headRef == NULL)
    {
        return NULL;
    }
    while (clientListTemp != NULL)
    {
        jobTemp = clientListTemp->job;
        while (jobTemp != NULL)
        {
            pidlist[index] = jobTemp->jobId;
            jobTemp = jobTemp->next;
            index++;
        }

        clientListTemp = clientListTemp->next;
    }
    return pidlist;
}

/** This function creates a list of pids from the job list**/
int *getPidList(struct jobList **headRef)
{
    struct jobList *temp = *headRef;
    int *pidlist = (int *)malloc(sizeof(int) * 1000);
    int index = 0;

    if (headRef == NULL)
    {
        return NULL;
    }
    while (temp != NULL)
    {
        pidlist[index] = temp->jobId;
        temp = temp->next;
        (index)++;
    }
    return pidlist;
}

/** This function prints the job done message on the terminal**/
void printdone(struct jobList *root)
{
    if (root != NULL)
    {
        char buffer[100];
        sprintf(buffer, "[%d] %s %s %s\n", root->jobCount, root->jobSign, "Done", root->jobCommand);
        send(root->terminal, buffer, strlen(buffer), 0);
    }
}

struct jobList *newNode(int jobId, char *jobCommand, JobStatus jobStatus, int jobCount, struct processList *process, int terminal)
{
    struct jobList *job =
        (struct jobList *)
            malloc(sizeof(struct jobList));
    job->jobCommand = jobCommand;
    job->jobId = jobId;
    job->jobStatus = jobStatus;
    job->jobCount = jobCount;
    job->process = process;
    job->jobSign = "+";
    job->terminal = terminal;
    job->next = NULL;
    return job;
}

/** This function returns the string value of JobStatus enum **/
const char *getJobsStatus(JobStatus jobStatus)
{
    switch (jobStatus)
    {
    case RUNNING:
        return "Running";
    case STOPPED:
        return "Stopped";
    }
}

/** This function changes the sign of job from plus to minus**/
struct jobList *changeJobSign(struct jobList **root)
{
    struct jobList *temp = *root;
    temp->jobSign = "-";
    return temp;
}

/** Function to push the new job in the job list **/
void pushJob(struct jobList **root, int jobId, char *jobCommand, JobStatus jobStatus, int jobCount, struct processList *process, int terminal)
{
    struct jobList *newJob = newNode(jobId, jobCommand, jobStatus, jobCount, process, terminal);
    if (*root == NULL)
    {
        newJob->next = NULL;
    }
    else
    {
        newJob->next = *root;
        // *root = changeJobSign(root);
    }
    *root = newJob;
}

void pushClientJob(struct ClientJobList **clientRoot, struct jobList *job, int terminal, struct processList *process, JobStatus jobStatus, int jobNumber)
{

    if (*clientRoot == NULL)
    {
        struct ClientJobList *clinetNewJob =
            (struct ClientJobList *)
                malloc(sizeof(struct ClientJobList));

        pushJob(&clinetNewJob->job, process->cpid, process->processString, jobStatus, jobNumber, process, terminal);
        struct jobList *job = clinetNewJob->job;
        clinetNewJob->terminalID = terminal;
        clinetNewJob->next = NULL;
        *clientRoot = clinetNewJob;
    }
    else
    {
        struct ClientJobList *clinetNewJob =
            (struct ClientJobList *)
                malloc(sizeof(struct ClientJobList));
        clinetNewJob = search(clientRoot, terminal);
        if (clinetNewJob != NULL)
        {

            pushJob(&clinetNewJob->job, process->cpid, process->processString, jobStatus, jobNumber, process, terminal);
        }
        else
        {

            struct ClientJobList *clinetNewJob =
                (struct ClientJobList *)
                    malloc(sizeof(struct ClientJobList));
            pushJob(&clinetNewJob->job, process->cpid, process->processString, jobStatus, jobNumber, process, terminal);
            struct jobList *job = clinetNewJob->job;
            clinetNewJob->terminalID = terminal;
            clinetNewJob->next = *clientRoot;
            *clientRoot = clinetNewJob;
        }
    }
}

/** Function to pop(remove the job from the top of the list) the job from the job list**/
struct jobList *popJob(struct jobList **root)
{
    if (root == NULL)
    {
        return NULL;
    }
    struct jobList *temp = *root;
    *root = (*root)->next;
    return temp;
}

/** This function prints all the jobs with their status, command name, count and sign from the job list on the shell terminal**/
void printJobs(struct jobList *root, int terminalID)
{

    if (root != NULL)
    {
        printJobs(root->next, terminalID);
        char buffer[100];
        sprintf(buffer, "[%d]%s %s %s\n", root->jobCount, root->jobSign, getJobsStatus(root->jobStatus), root->jobCommand);
        send(terminalID, buffer, strlen(buffer), 0);
    }
}

void printClientJobs(struct ClientJobList *root, int psd)
{

    if (!isEmpty(root->job))
    {
        printJobs(root->job, root->terminalID);
    }
    else
    {
        struct ClientJobList *clientList = NULL;
        clientList = search(&rootClientJobList, psd);
        if (clientList != NULL)
            clientList->clientJobNumber = 0;
    }
}

/** This function deletes the fisrt available job with the given status from the job list **/
struct jobList *deleteJobByStatus(struct jobList **headRef, JobStatus jobStatus)
{

    struct jobList *temp = *headRef, *prev;
    int jobid;
    if (temp != NULL && temp->jobStatus == jobStatus)
    {
        *headRef = temp->next;
        return temp;
    }

    while (temp != NULL && temp->jobStatus != jobStatus)
    {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
        return NULL;

    prev->next = temp->next;
    return temp;
}

void deleteByPIDClientList(struct ClientJobList **rootClientJobList, int pid, int psd)
{
    struct ClientJobList *tempClientJobList = *rootClientJobList;
    struct jobList *tempJobList = NULL;
    while (tempClientJobList != NULL)
    {
        tempJobList = deleteJobByPid(&tempClientJobList->job, pid, psd);
        tempClientJobList = tempClientJobList->next;
    }
}

/** This function deletes the job with the given pid from the job list **/
struct jobList *deleteJobByPid(struct jobList **headRef, int pid, int psd)
{

    struct jobList *temp = *headRef, *prev;
    int jobid;
    if (temp != NULL && temp->jobId == pid)
    {
        *headRef = temp->next;
        struct ClientJobList *clientList = NULL;
        clientList = search(&rootClientJobList, psd);
        if (clientList != NULL && temp->jobCount == clientList->clientJobNumber)
        {
            checkPrevJobCount(&temp, psd);
        }
        printdone(temp);
        return temp;
    }

    while (temp != NULL && temp->jobId != pid)
    {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
        return NULL;

    prev->next = temp->next;
    struct ClientJobList *clientList = NULL;
    clientList = search(&rootClientJobList, psd);
    if (clientList != NULL && temp->jobCount == clientList->clientJobNumber)
    {
        checkPrevJobCount(&temp, psd);
    }
    printdone(temp);
    return temp;
}

/** This function assigns job signs to the jobs before printing them on the terminal**/
void assigbJobSign(struct jobList **jobRef)
{
    struct jobList *curNode = *jobRef;
    struct jobList *prevNode = NULL;
    if (!isEmpty(*jobRef))
    {
        if (curNode != NULL)
        {
            curNode->jobSign = "+";
        }
        while (curNode->next != NULL)
        {

            curNode = curNode->next;
            curNode->jobSign = "-";
        }
    }
}

/**This function updates the globalJobNumber once the job with highest job number gets completed **/
void checkPrevJobCount(struct jobList **temp, int psd)
{
    struct jobList *prevNode = NULL;
    struct jobList *curNode = *temp;
    struct ClientJobList *clientList = NULL;
    clientList = search(&rootClientJobList, psd);
    if (clientList != NULL)
    {
        if (curNode->next != NULL)
        {
            prevNode = curNode->next;
            clientList->clientJobNumber = prevNode->jobCount;
        }
        else
        {
            clientList->clientJobNumber = 0;
        }
    }
}

/** This function does the initianlization of the shell and handles the signal for yash **/
void initshell()
{

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    pid_t pid = getpid();
    setpgid(pid, pid);
    // tcsetpgrp(0, pid);
}

/**To check if the given command is shell command**/
int checkIfShellCommands(char *inString)
{
    int isShellCommands = 1;
    if (strstr(inString, "bg") || strstr(inString, "fg") || strstr(inString, "jobs") || strstr(inString, "exit"))
    {
        isShellCommands = 0;
        return isShellCommands;
    }

    return isShellCommands;
}

/** Parse the string with the given delimiter**/
char **parseStringStrtok(char *str, char *delim)
{

    char **parsedString = (char **)malloc(sizeof(char) * 1000);
    char *saveptr1;
    char *token = (char *)malloc(sizeof(char) * 1000);
    token = strtok_r(str, delim, &saveptr1);
    int index = 0;
    while (token != NULL)
    {
        parsedString[index++] = token;
        token = strtok_r(NULL, delim, &saveptr1);
    }
    return parsedString;
}

/**This function is to parse the command for redirection and set inuput/output file parameters in the process list**/
struct processList *parseSubCommand(char **subCommand)
{
    int index = 0;
    char *inputPath = NULL;
    char *outputPath = NULL;
    char **mainSubCommand = (char **)malloc(sizeof(char) * 100);
    int mainIndex = 0;
    int pos1 = -1;
    int pos2 = -1;

    index = 0;
    while (subCommand[index] != NULL)
    {
        if ((strstr(subCommand[index], "<") || strstr(subCommand[index], ">")) && (pos1 == -1))
        {
            pos1 = index;
            index++;
            continue;
        }
        if ((strstr(subCommand[index], "<") || strstr(subCommand[index], ">")) && (pos1 > 0) && (pos2 == -1))
        {
            pos2 = index;
            break;
        }
        index++;
    }

    if (pos2 > 0)
    {
        if (strstr(subCommand[pos1], "<"))
        {
            inputPath = (char *)malloc(100 * sizeof(char));
            inputPath = subCommand[pos1 + 1];
            index = 0;
            while (index < pos1)
            {
                mainSubCommand[mainIndex++] = subCommand[index];
                index++;
            }
        }
        else if (strstr(subCommand[pos2], "<"))
        {
            inputPath = (char *)malloc(100 * sizeof(char));
            inputPath = subCommand[pos2 + 1];
        }

        if (strstr(subCommand[pos1], ">"))
        {
            outputPath = (char *)malloc(100 * sizeof(char));
            outputPath = subCommand[pos1 + 1];
            index = 0;
            while (index < pos1)
            {
                mainSubCommand[mainIndex++] = subCommand[index];
                index++;
            }
        }
        else if (strstr(subCommand[pos2], ">"))
        {
            outputPath = (char *)malloc(100 * sizeof(char));
            outputPath = subCommand[pos1 + 1];
        }
    }
    else if ((pos2 < 0) && (pos1 > 0))
    {
        if (strstr(subCommand[pos1], "<"))
        {
            inputPath = (char *)malloc(100 * sizeof(char));
            inputPath = subCommand[pos1 + 1];
            index = 0;
            while (index < pos1)
            {
                mainSubCommand[mainIndex++] = subCommand[index];
                index++;
            }
        }
        else if (strstr(subCommand[pos1], ">"))
        {
            outputPath = (char *)malloc(100 * sizeof(char));
            outputPath = subCommand[pos1 + 1];
            index = 0;
            while (index < pos1)
            {
                mainSubCommand[mainIndex++] = subCommand[index];
                index++;
            }
        }
    }
    else if ((pos2 < 0) && (pos1 < 0))
    {
        index = 0;
        while (subCommand[index] != NULL)
        {
            mainSubCommand[mainIndex++] = subCommand[index];
            index++;
        }
    }
    mainSubCommand[index] = 0;

    struct processList *process =
        (struct processList *)
            malloc(sizeof(struct processList));

    process->inputPath = inputPath;
    process->outputPath = outputPath;
    process->processArgs = mainSubCommand;
    process->next = NULL;
    return process;
}

/**This function checks if the given command has a pipe; if yes then parse the left and right child of the command
 * separatly otherwise pass the entire command at once**/
struct processList *parseStringforPipes(char **parsedCmdArray)
{

    char **lchild = (char **)malloc(sizeof(char) * 1000);
    char **rchild = (char **)malloc(sizeof(char) * 1000);
    // int lchildindex = 0;
    int index = 0;
    struct processList *parentProcess = NULL;
    struct processList *leftChildProcess = NULL;
    struct processList *rightChildProcess = NULL;

    int cmdArrayLength = 0;
    while (parsedCmdArray[cmdArrayLength] != NULL)
    {
        cmdArrayLength++;
    }

    while (parsedCmdArray[index] != NULL)
    {
        if (strstr(parsedCmdArray[index], "|"))
        {
            int lchildindex = 0;
            while (lchildindex < index)
            {
                lchild[lchildindex] = strdup(parsedCmdArray[lchildindex]);
                lchildindex++;
            }
            lchild[lchildindex] = NULL;
            int rchildindex = 0;
            int rindex = index + 1;
            while (rindex < cmdArrayLength)
            {
                rchild[rchildindex] = strdup(parsedCmdArray[rindex]);
                rchildindex++;
                rindex++;
            }
            rchild[rchildindex] = NULL;

            struct processList *leftChildProcess = parseSubCommand(lchild);
            struct processList *rightChildProcess = parseSubCommand(rchild);

            parentProcess = leftChildProcess;
            leftChildProcess->next = rightChildProcess;
            break;
        }
        index++;
    }

    if (parentProcess == NULL)
    {
        struct processList *process = parseSubCommand(parsedCmdArray);
        parentProcess = process;
    }

    return parentProcess;
}

/** This function returns count of process currenlty in execution**/
int getProcessCount(struct processList *rootProcess)
{
    int count = 0;
    struct processList *processObj = (struct processList *)
        malloc(sizeof(struct processList));

    for (processObj = rootProcess; processObj != NULL; processObj = processObj->next)
    {
        count++;
    }

    return count;
}

/**This function executes the parsed command by using fork and execvp; This function accepts the job type
 * as a parameter and runs the command
 * in foreground or background mode depending on that**/

int exexuteCommands(struct processList *rootProcess, int infd, int outfd, int job_type, int psd, struct jobList **rootJob, struct sockaddr_in cliaddr)
{

    int status = 0;
    int cpid;
    int pipefd_2[2];
    pipe(pipefd_2);
    
    cpid = fork();

    if (cpid < 0)
    {
        return -1;
    }

    if (cpid == 0)
    {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGSTOP, SIG_DFL);

        close(pipefd_2[1]);
        dup2(pipefd_2[0], STDIN_FILENO);

        rootProcess->cpid = getpid();
        if (rootProcess->groupId > 0)
        {
            setpgid(0, rootProcess->groupId);
        }
        else
        {
            rootProcess->groupId = rootProcess->cpid;
            setpgid(0, rootProcess->groupId);
        }

        if (infd != 0)
        {         
            dup2(infd, STDIN_FILENO);
            close(infd);
        }
        if (outfd != 1)
        {
            dup2(outfd, psd);
            close(outfd);
        }
        close(STDOUT_FILENO);
        dup2(psd, STDOUT_FILENO);
        dup2(psd, STDERR_FILENO);

        if (execvp(rootProcess->processArgs[0], rootProcess->processArgs) < 0)
        {
            exit(0);
        }

        exit(0);
    }
    else
    {
        // parent process
        rootProcess->cpid = cpid;
        if (rootProcess->groupId > 0)
        {
            setpgid(cpid, rootProcess->groupId);
        }
        else
        {
            rootProcess->groupId = rootProcess->cpid;
            setpgid(cpid, rootProcess->cpid);
        }

        if (job_type == fg)
        {
            int waitCount = 0;
            //siginfo_t SignalInfo;
            int processCount = getProcessCount(rootProcess);
            int waitStatus;

            close(pipefd_2[0]);
            // dup2(pipefd_2[1], STDOUT_FILENO); 
    
            waitFunction(rootProcess->groupId, psd, &waitStatus, cliaddr, outfd, pipefd_2[1]);

            if (WIFSTOPPED(waitStatus))
            {
                struct ClientJobList *clientList = NULL;
                clientList = search(&rootClientJobList, psd);
                printf("stopped by signal %d\n", WSTOPSIG(waitStatus));
                int jobNumber = 0;
                if (clientList != NULL && !isEmpty(clientList->job))
                {
                    clientList->clientJobNumber = clientList->clientJobNumber + 1;
                    jobNumber = clientList->clientJobNumber;
                }
                pushClientJob(&rootClientJobList, *rootJob, psd, rootProcess, STOPPED, ++jobNumber);
            }

            //signal(SIGTTOU, SIG_IGN);
            // tcsetpgrp(0, getpid());
            //signal(SIGTTOU, SIG_DFL);
        }
        else if (job_type == bg)
        {

            struct ClientJobList *clientList = NULL;

            clientList = search(&rootClientJobList, psd);
            int jobNumber = 0;
            if (clientList != NULL && !isEmpty(clientList->job))
            {
                clientList->clientJobNumber = clientList->clientJobNumber + 1;
                jobNumber = clientList->clientJobNumber;
            }
            pushClientJob(&rootClientJobList, *rootJob, psd, rootProcess, RUNNING, ++jobNumber);
        }
    }
    return status;
}

void waitFunction(int pid, int psd, int *waitStatus, struct sockaddr_in cliaddr, int outfd, int inputEnd)
{
    pthread_t waitThread;
    int waitid1 = 0;
    waitStruct args;
    args.psd = psd;
    args.groupId = pid;
    args.doneFlag = 1;
    args.cliAddr = cliaddr;
    args.outfd = outfd;
    args.inputEnd = inputEnd;

    if (pthread_create(&waitThread, NULL, waitingThread, &args) < 0)
    {
        perror("error thread");
    }

    waitid1 = waitpid(-pid, waitStatus, WUNTRACED);
    args.doneFlag = 2;
    pthread_cancel(waitThread);
    pthread_join(waitThread, NULL);
}

void *waitingThread(void *param)
{
    char buf[BUFSIZE];
    int rc;
    waitStruct *mycmd = (waitStruct *)param;
    int psd = mycmd->psd;
    int pid = mycmd->groupId;
    struct sockaddr_in cliAddr;
    cliAddr = mycmd->cliAddr;
    int outfd = mycmd->outfd;
    int inputEnd = mycmd->inputEnd;

    socklen_t fromlen = sizeof(cliAddr);
    dprintf(2, "inside waiting thread block\n");

    while (mycmd->doneFlag == 1)
    {
        cleanup(buf);
        memset(buf, 0, sizeof(buf));        
        dprintf(2, "inside while thread %d\n", mycmd->doneFlag);
        if ((rc = recvfrom(psd, (char *)buf, BUFSIZE, 0, (struct sockaddr *)&cliAddr, &fromlen)) < 0) //blocking 
        {
            perror("receiving stream  message 2");
            pthread_exit((void *)1);
        }
        if (rc >= 0)
        {
            dprintf(2, "inside check rc\n");
            buf[rc] = '\0';
            char *inString;
            inString = strdup(buf);
            checkForCTLcmd(inString, pid, &(mycmd->doneFlag), outfd, psd, cliAddr, rc, inputEnd);
        }
        else
        {
            dprintf(2, "after pthread_exit \n");
            pthread_exit(0);
            pthread_cancel(pthread_self());
        }
    }
    dprintf(2, "last pthread exit \n");
    pthread_exit(0);
}

char *manageCommand(char *cmd)
{
    char *incomingCmd = strdup(cmd);
    if (strstr(cmd, "CMD"))
    {
        char *savePtr;
        char *newCmd = strtok_r(cmd, "CMD", &savePtr);

        return newCmd;
    }
    return NULL;
}

void checkForCTLcmd(char *cmd, int pid, int *doneFlag, int outfd, int psd, struct sockaddr_in cliAddr, int rc, int inputEnd)
{
    dprintf(2, "inside checkForCTLcmd block\n");
    if (strstr(cmd, "CTL"))
    {
         dprintf(2, "inside checkForCTLcmd if block\n");
        *doneFlag = 3;
        if (strstr(cmd, "CTL C") || strstr(cmd, "CTL c"))
        {
            kill(pid, SIGINT);
            pthread_exit(0);
        }
        else if (strstr(cmd, "CTL Z") || strstr(cmd, "CTL z"))
        {
            kill(pid, SIGTSTP);
            pthread_exit(0);
        }
    }
    else {
        dprintf(2, "inside checkForCTLcmd else block\n");
        char *firstLine = strdup(cmd);
        char *newCmd = manageCommand(firstLine);
        dprintf(2, "%s\n", newCmd);
        write(inputEnd, newCmd, strlen(newCmd));
    }

}
/**This function sets the input/output/pipe  parameter before execution**/
int executeParsedCommand(struct processList *rootProcess, int job_type, int psd, struct jobList **rootJob, struct sockaddr_in cliaddr)
{
    printZombies(psd);
    struct processList *proc;
    int status = 0;
    int pipefd[2];
    int infd = 0;
    int outfd = 1;
    pipe(pipefd);
    while (rootProcess != NULL)
    {
        if (rootProcess->inputPath != NULL )
        {
            if ((infd = open(rootProcess->inputPath, O_RDONLY)) < 0)
            {
                fprintf(stderr, "error opening file\n");
            }

        }

        if (rootProcess->outputPath != NULL)
        {
            outfd = 1;
            pthread_mutex_lock(&lock);
            outfd = open(rootProcess->outputPath, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (outfd < 0)
            {
                outfd = 1;
            }
            pipefd[1] = outfd;
            pthread_mutex_unlock(&lock);
        }

        if (rootProcess->next == NULL)
        {
            status = exexuteCommands(rootProcess, infd, outfd, job_type, psd, rootJob, cliaddr);
        }
        else if (rootProcess->next != NULL)
        {
            status = exexuteCommands(rootProcess, infd, pipefd[1], 2, psd, rootJob, cliaddr);    
            close(pipefd[1]);
            infd = pipefd[0];
        }        

        rootProcess = rootProcess->next;
    }

    return status;
}

/**This function executes shell commands like fg/bg/jobs**/
void executeShellCommands(char *inString, int psd, struct jobList **rootJob, struct sockaddr_in cliaddr)
{
    char *command = strdup(inString);
    if (strstr(inString, "fg"))
    {

        struct jobList *jobObj =
            (struct jobList *)
                malloc(sizeof(struct jobList));
        if (!isEmpty(*rootJob))
        {
            jobObj = popJob(rootJob);
            pid_t pid;
            pid = jobObj->process->groupId;

            usleep(100);
            if (jobObj->jobStatus != RUNNING)
            {
                dprintf(psd, "%s\n", jobObj->jobCommand);
                kill(-pid, SIGCONT);
            }
            else
            {
                dprintf(psd, "%s\n", jobObj->jobCommand);
            }
            // tcsetpgrp(0, pid);

            int status = 0;
            // pthread_mutex_lock(&lock);
            waitFunction(pid, psd, &status, cliaddr, 2, 0);

            if (WSTOPSIG(status))
            {
                pushJob(rootJob, jobObj->process->cpid, jobObj->jobCommand, STOPPED, jobObj->jobCount, jobObj->process, psd);
            }
            // pthread_mutex_unlock(&lock);

            signal(SIGTTOU, SIG_IGN);
            // tcsetpgrp(0, getpid());
            signal(SIGTTOU, SIG_DFL);
        }
    }
    else if (strstr(inString, "bg"))
    {

        struct jobList *jobObj =
            (struct jobList *)
                malloc(sizeof(struct jobList));

        jobObj = deleteJobByStatus(rootJob, STOPPED);

        if (jobObj != NULL)
        {
            int wpid = -1;
            pid_t pid;
            pid = jobObj->process->groupId;
            int wstatus;

            kill(-pid, SIGCONT);
            usleep(100);
            wpid = waitpid(-1, &wstatus, WUNTRACED | WCONTINUED);
            if (WIFCONTINUED(wstatus))
            {
                char buffer[100];
                pushJob(rootJob, jobObj->process->cpid, jobObj->jobCommand, RUNNING, jobObj->jobCount, jobObj->process, psd);
                sprintf(buffer, "[%d] %s %s\n", jobObj->jobCount, jobObj->jobSign, jobObj->jobCommand);
                send(psd, buffer, strlen(buffer), 0);
            }
        }
    }
    else if (strstr(inString, "jobs"))
    {

        if (!isEmptyClient(rootClientJobList))
        {

            struct ClientJobList *clientList = NULL;
            clientList = search(&rootClientJobList, psd);
            if (clientList != NULL)
            {
                assigbJobSign(&clientList->job);
                printClientJobs(clientList, psd);
            }
        }
    }
    else if (strstr(inString, "exit"))
    {
        return; // exit(0);
    }
}

/**This function checks if the given command is background command, if yes then
 * removes the & from the cmd string and set bg flag and then parse rest of the command by removing
 * spaces from the commands and collects token **/
char **parsecommands(char *inString, int psd, struct jobList **rootJob, struct sockaddr_in cliaddr)
{

    // globalPsd = psd;
    printZombies(psd);
    char *command = strdup(inString);
    int job_type = fg;
    char **parsedCommandsArray;
    int status = 0;
    struct processList *rootProcess = NULL;

    if (inString[strlen(inString) - 1] == '&')
    {
        job_type = bg;
        inString[strlen(inString) - 1] = '\0';
    }

    if (checkIfShellCommands(inString) == 0)
    {
        if (!isEmptyClient(rootClientJobList))
        {

            struct ClientJobList *clientList = NULL;
            clientList = search(&rootClientJobList, psd);
            if (clientList != NULL)
            {
                executeShellCommands(inString, psd, &clientList->job, cliaddr);
            }
        }
    }
    else
    {
        parsedCommandsArray = parseStringStrtok(inString, " ");
        int index = 0;
        while (parsedCommandsArray[index] != NULL)
        {
            index++;
        }
        parsedCommandsArray[index] = 0;

        rootProcess = parseStringforPipes(parsedCommandsArray);
        rootProcess->processString = command;

        status = executeParsedCommand(rootProcess, job_type, psd, rootJob, cliaddr);
    }
    return parsedCommandsArray;
}
