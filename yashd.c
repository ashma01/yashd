#include <stdio.h>
/* socket(), bind(), recv, send */
#include <sys/types.h>
#include <sys/socket.h> /* sockaddr_in */
#include <netinet/in.h> /* inet_addr() */
#include <arpa/inet.h>
#include <netdb.h>  /* struct hostent */
#include <string.h> /* memset() */
#include <unistd.h> /* close() */
#include <stdlib.h> /* exit() */
#include "yash.h"

extern int errno;

#define PATHMAX 255
#define MAXHOSTNAME 80
#define PORT 7878

static char u_server_path[PATHMAX + 1] = "/tmp"; /* default */
static char u_socket_path[PATHMAX + 1];
static char u_log_path[PATHMAX + 1];
static char u_pid_path[PATHMAX + 1];

void reusePort(int s);
void execute(char *inString, int psd, struct jobList **rootJob);
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

void sig_chld(int n)
{
    int status;

    dprintf(2, "Child terminated\n");
    wait(&status);
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

    /* the child */

    /* Close all file descriptors that are open */
    for (k = getdtablesize() - 1; k > 0; k--)
        close(k);

    /* Redirecting stdin and stdout to /dev/null */
    if ((fileFd = open("/dev/null", O_RDWR)) < 0)
    {
        perror("Open");
        exit(0);
    }
    dup2(fileFd, STDIN_FILENO);  /* detach stdin */
    dup2(fileFd, STDOUT_FILENO); /* detach stdout */
    close(fileFd);
    /* From this point on printf and scanf have no effect */

    /* Redirecting stderr to u_log_path */
    log = fopen(u_log_path, "aw"); /* attach stderr to u_log_path */
    fileFd = fileno(log);          /* obtain file descriptor of the log */
    dup2(fileFd, STDERR_FILENO);
    close(fileFd);
    /* From this point on printing to stderr will go to /tmp/u-echod.log */

    /* Change directory to specified directory */
    chdir(path);

    /* Set umask to mask (usually 0) */
    umask(mask);

    /* Detach controlling terminal by becoming sesion leader */
    sid = setsid();
    if (sid < 0)
    {
        perror("setsid() failure! \n");
        exit(1);
    }

    /* Put self in a new process group */
    pid = getpid();
    setpgrp(); /* GPI: modified for linux */

    /* Make sure only one server is running */
    if ((k = open(u_pid_path, O_RDWR | O_CREAT, 0666)) < 0)
        exit(1);
    if (lockf(k, F_TLOCK, 0) != 0)
        exit(0);

    /* Save server's pid without closing file (so lock remains)*/
    sprintf(buff, "%6d", pid);
    write(k, buff, strlen(buff));

    signal(SIGTERM, sig_exit);
    return;
}

void makeConnections(char *argv[])
{
    int sd;
    struct sockaddr_in server;
    struct hostent *hp, *gethostbyname();
    struct sockaddr_in from;
    socklen_t fromlen;
    socklen_t length;
    char ThisHost[80];
    int pn, i = 0;
    int childpid;

    struct ClientInfo client;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, sigChildHandler);
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, sig_hup);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        dprintf(2, "failed to create socket");
        exit(-1);
    }

    reusePort(sd);

    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        close(sd);
        dprintf(2, "binding name to stream socket");
        exit(-1);
    }

    length = sizeof(server);
    if (getsockname(sd, (struct sockaddr *)&server, &length))
    {
        perror("getting socket name");
        exit(0);
    }

    pthread_mutex_init(&lock, NULL);
    pthread_t thread_id;
    listen(sd, 4);
    fromlen = sizeof(from);

    for (;;)
    {
        int psd = accept(sd, (struct sockaddr *)&from, &fromlen);
        if (psd == -1)
        {
            perror("connection error");
        }
        else
        {
            client.sock = psd;
            client.from = from;

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
    struct sockaddr_in from;
    struct jobList *rootJob = NULL;
    struct ClientInfo *tempInfo = (struct ClientInfo *)clientInfo;
    psd = tempInfo->sock;
    from = tempInfo->from;
    struct ClientInfo info;
    info = *tempInfo;

    // pthread_mutex_init(&info.lock, NULL);
    struct hostent *hp, *gethostbyname();

    for (;;)
    {
        cleanup(buf);
        dprintf(psd, "\n....server is waiting...\n");
        // char *start = "\n# ";
        // send(psd, start, strlen(start), 0);

        if ((rc = recv(psd, buf, sizeof(buf), 0)) < 0)
        {
            perror("receiving stream  message");
            pthread_exit((void *)1);
        }
        if (rc > 0)
        {
            buf[rc] = '\0';
            char *inString;
            inString = strdup(buf);
            logging(from, inString);
            execute(inString, psd, &rootJob);
        }
        else
        {
            dprintf(2, "Client: %s:%d Disconnected..\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
            fflush(stdout);
            close(psd);
            pthread_exit(0);
        }
    }
}

void logging(struct sockaddr_in from, char *inString)
{

    // time_t currTime;
    // time(&currTime);
    // char *newCmd;
    if (strstr(inString, "CMD"))
    {
        dprintf(2, "yashd[%s:%d]: %s\n", inet_ntoa(from.sin_addr),
                ntohs(from.sin_port), inString);
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

void execute(char *inString, int psd, struct jobList **rootJob)
{
    char *command;
    char *newCmd;
    command = strdup(inString);
    if (inString != NULL)
    {
        newCmd = manageCommandOrSignals(inString);
        if (newCmd != NULL)
        {
            // pthread_mutex_lock(&lock);
            char **resultString = parsecommands(newCmd, psd, rootJob);
            // pthread_mutex_unlock(&lock);
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

    // initializeDaemon(u_server_path, 0); /* We stay in the u_server_path directory and file
    //                                   creation is not restricted. */

    unlink(u_socket_path); /* delete the socket if already existing */

    makeConnections(argv);
}
