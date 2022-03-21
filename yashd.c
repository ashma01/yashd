#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "yash.h"

extern int errno;

#define PATHMAX 255
#define MAXHOSTNAME 80
#define PORT 3826

static char u_server_path[PATHMAX + 1] = "/tmp"; /* default */
static char u_socket_path[PATHMAX + 1];
static char u_log_path[PATHMAX + 1];
static char u_pid_path[PATHMAX + 1];

void reusePort(int s);
void execute(char *inString, int psd, struct jobList **rootJob, struct sockaddr_in cliAddr);
char *manageCommandOrSignals(char *cmd);
void *processRequest(void *input);
void initializeDaemon(const char *const path, uint mask);
void makeConnections(char *argv[]);
void logging(struct sockaddr_in from, char *inString);

struct ClientInfo
{
    int sock;
    struct sockaddr_in from;
};

void sig_pipe(int n)
{
    perror("Broken pipe signal");
}

void sig_hup(int n)
{
    int status;

    dprintf(2, "Child hup\n");
    wait(&status);
}
void sig_exit(int signo)
{

    dprintf(2, "exiting\n");
    exit(EXIT_SUCCESS);
}

void initializeDaemon(const char *const path, uint mask)
{
    pid_t pid;
    pid_t sid;
    char buff[256];
    static FILE *log;
    int fileFd;
    int k;

    if ((pid = fork()) < 0)
    {
        perror("initializeDaemon: cannot fork");
        exit(0);
    }
    else if (pid > 0) /* The parent */
        exit(0);

    for (k = getdtablesize() - 1; k > 0; k--)
        close(k);

    if ((fileFd = open("/dev/null", O_RDWR)) < 0)
    {
        perror("Open");
        exit(0);
    }
    dup2(fileFd, STDIN_FILENO);  /* detach stdin */
    dup2(fileFd, STDOUT_FILENO); /* detach stdout */
    close(fileFd);

    // log = fopen(u_log_path, "aw");
    // fileFd = fileno(log);
    // dup2(fileFd, STDERR_FILENO);
    // close(fileFd);

    /* Establish handlers for signals */
    // initshell();
    // signal(SIGINT, SIG_IGN);
    // signal(SIGQUIT, SIG_IGN);
    // signal(SIGTSTP, SIG_IGN);
    // signal(SIGTTIN, SIG_IGN);
    // signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    if (signal(SIGPIPE, sig_pipe) < 0)
    {
        perror("Signal SIGPIPE");
        exit(1);
    }

    /* Change directory to specified directory */
    chdir(path);

    /* Set umask to mask (usually 0) */
    umask(mask);

    sid = setsid();
    if (sid < 0)
    {
        perror("setsid() failure! \n");
        exit(1);
    }

    pid = getpid();
    setpgrp();

    if ((k = open(u_pid_path, O_RDWR | O_CREAT, 0666)) < 0)
        exit(1);
    if (lockf(k, F_TLOCK, 0) != 0)
        exit(0);

    sprintf(buff, "%6d\n", pid);
    write(k, buff, strlen(buff));

    signal(SIGTERM, sig_exit);
    return;
}

void makeConnections(char *argv[])
{
    int sd;
    struct sockaddr_in serverAddr;
    struct hostent *hp, *gethostbyname();
    struct sockaddr_in clientAddr;
    socklen_t fromlen;
    socklen_t length;
    char ThisHost[80];
    int pn, i = 0;
    int childpid;

    // initshell();

    memset(&serverAddr, 0, sizeof(serverAddr));
    memset(&clientAddr, 0, sizeof(clientAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(PORT);
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        dprintf(2, "failed to create socket");
        exit(-1);
    }

    reusePort(sd);

    if (bind(sd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        close(sd);
        dprintf(2, "binding name to stream socket");
        exit(-1);
    }

    length = sizeof(serverAddr);
    if (getsockname(sd, (struct sockaddr *)&serverAddr, &length))
    {
        perror("getting socket name");
        exit(0);
    }

    pthread_mutex_init(&lock, NULL);
    pthread_t thread_id;
    listen(sd, 4);
    fromlen = sizeof(clientAddr);

    for (;;)
    {
        int psd = accept(sd, (struct sockaddr *)&clientAddr, &fromlen);
        if (psd == -1)
        {
            perror("connection error");
        }
        else
        {
            struct ClientInfo client;
            client.sock = psd;
            client.from = clientAddr;

            if (pthread_create(&thread_id, NULL, processRequest, (void *)&client) < 0)
            {
                perror("error thread");
            }
            else
            {
                pthread_detach(thread_id);
            }
        }
    }
    close(sd);
    pthread_mutex_destroy(&lock);
    return;
}

void *processRequest(void *clientInfo)
{

    char buf[BUFSIZE];
    int rc;
    int psd;
    struct sockaddr_in cliaddr;
    struct jobList *rootJob = NULL;
    struct ClientInfo *tempInfo = (struct ClientInfo *)clientInfo;
    psd = tempInfo->sock;
    cliaddr = tempInfo->from;
    struct ClientInfo info;
    info = *tempInfo;

    // pthread_mutex_init(&info.lock, NULL);
    struct hostent *hp, *gethostbyname();
    socklen_t fromlen = sizeof(cliaddr);

    for (;;)
    {
        int bytes_sent;
        cleanup(buf);
        memset(buf, 0, sizeof(buf));
        char start[] = "\n#\n";
        write(psd, start, strlen(start));

        if ((rc = recvfrom(psd, (char *)buf, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &fromlen)) < 0)
        {
            perror("receiving stream  message 1");
            pthread_exit((void *)1);
        }
        if (rc > 0)
        {
            buf[rc] = '\0';
            char *inString;
            inString = strdup(buf);
            pthread_mutex_lock(&lock);
            logging(cliaddr, inString);
            pthread_mutex_unlock(&lock);
            execute(inString, psd, &rootJob, cliaddr);
        }
        else
        {
            dprintf(2, "Client: %s:%d Disconnected..\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
            close(psd);
            pthread_exit(0);
        }
    }
}

void logging(struct sockaddr_in from, char *inString)
{
    static FILE *log;
    char cur_time[128];
    time_t t;
    struct tm *ptm;

    t = time(NULL);
    ptm = localtime(&t);

    strftime(cur_time, 128, "%d-%b-%Y %H:%M:%S", ptm);

    log = fopen(u_log_path, "aw");

    if (strstr(inString, "CMD"))
    {

        fprintf(log, "%s yashd[%s:%d]: %s\n", cur_time, inet_ntoa(from.sin_addr),
                ntohs(from.sin_port), inString);
        fclose(log);
    }
}
void reusePort(int s)
{
    int one = 1;

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) == -1)
    {
        perror("error in setsockopt,SO_REUSEPORT \n");
        exit(-1);
    }
}

char *manageCommandOrSignals(char *cmd)
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

void execute(char *inString, int psd, struct jobList **rootJob, struct sockaddr_in cliaddr)
{
    char *command;
    char *newCmd;
    command = strdup(inString);
    if (inString != NULL)
    {
        newCmd = manageCommandOrSignals(inString);
        if (newCmd != NULL)
        {

            char **resultString = parsecommands(newCmd, psd, rootJob, cliaddr);
        }
    }
}
int main(int argc, char *argv[])
{

    /* Initialize path variables */
    if (argc > 1)
        strncpy(u_server_path, argv[1], PATHMAX); /* use argv[1] */
    strncat(u_server_path, "/", PATHMAX - strlen(u_server_path));
    strncat(u_server_path, argv[0], PATHMAX - strlen(u_server_path));
    strcpy(u_socket_path, u_server_path);
    strcpy(u_pid_path, u_server_path);
    strncat(u_pid_path, ".pid", PATHMAX - strlen(u_pid_path));
    strcpy(u_log_path, u_server_path);
    strncat(u_log_path, ".log", PATHMAX - strlen(u_log_path));

    initializeDaemon(u_server_path, 0); /* We stay in the u_server_path directory and file  creation is not restricted. */

    unlink(u_socket_path); /* delete the socket if already existing */

    makeConnections(argv);
}
