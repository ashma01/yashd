#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "yashShell.h"

extern int errno;

#define PATHMAX 255
#define MAXHOSTNAME 80
#define PORT 3826

char *getcwd(char *buf, size_t size);
static char *pid_file_name = "/tmp/yashd.pid";
static char *log_file_name = "/tmp/yashd.log";

void reusePort(int s);
void execute(char *inString, int psd, struct jobList **rootJob, struct sockaddr_in cliAddr);
char *manageCommandOrSignals(char *cmd);
void *processRequest(void *input);
static void daemonize(char *cwd);
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

static void daemonize(char *cwd)
{
    pid_t pid;
    char buff[100];
    int pid_fd = -1;

    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    if (setsid() < 0)
        exit(EXIT_FAILURE);

    signal(SIGHUP, SIG_IGN);

    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);

    chdir(cwd);

    int x;
    for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
    {
        close(x);
    }

    stdin = fopen("/dev/null", "r");
    stdout = fopen("/dev/null", "w+");
    stderr = fopen("/dev/null", "w+");
    if (pid_file_name != NULL)
    {
        char str[256];
        pid_fd = open(pid_file_name, O_RDWR | O_CREAT, 0640);
        if (pid_fd < 0)
        {

            exit(EXIT_FAILURE);
        }
        if (lockf(pid_fd, F_TLOCK, 0) < 0)
        {

            exit(EXIT_FAILURE);
        }
        sprintf(str, "%d\n", getpid());
        write(pid_fd, str, strlen(str));
    }
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

    memset(&serverAddr, 0, sizeof(serverAddr));
    memset(&clientAddr, 0, sizeof(clientAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(PORT);
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        perror("failed to create socket");
        exit(-1);
    }

    reusePort(sd);

    if (bind(sd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        close(sd);
        perror("binding name to stream socket");
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
            perror("receiving stream  message");
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

            logging(cliaddr, "Client Disconnected");
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

    log = fopen(log_file_name, "aw");

    if (inString != NULL)
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
    char cwd[PATHMAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd() error");
    }
    daemonize(cwd);
    makeConnections(argv);
}
