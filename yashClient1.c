/**
 * @file TCPClient2-ex2.c
 * @brief The program creates a stream socket
 * in the inet domain, Connect to TCPServer2, Get messages typed by a
 * user and Send them to TCPServer2 running on hostid Then it waits
 * for a reply from the TCPServer2 and show it back to the user, with
 * a message indicating if there is an error during the round trip
 * Run as:
 *   TCPClient-ex2 <hostname> <port>
 */
#include <readline/readline.h>
#include <readline/history.h>
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
#include <signal.h>

#define MAXHOSTNAME 80
#define BUFSIZE 1024

char buf[BUFSIZE];
char rbuf[BUFSIZE];
void GetUserInput();
void cleanup(char *buf);
char *updateCommands(char *inString, char *cmdType);

int rc, cc;
int sd;

static void sig_int(int signo)
{

    char *finalString = (char *)malloc(sizeof(char) * 1000);
    finalString = updateCommands("C", "CTL");
    if (send(sd, finalString, strlen(finalString), 0) < 0)
        perror("sending stream message");
}

static void sig_tstp(int signo)
{

    char *finalString = (char *)malloc(sizeof(char) * 1000);
    finalString = updateCommands("Z", "CTL");
    if (send(sd, finalString, strlen(finalString), 0) < 0)
        perror("sending stream message");
}

int main(int argc, char **argv)
{
    int childpid;
    struct sockaddr_in server;
    struct sockaddr_in client;
    struct hostent *hp, *gethostbyname();
    struct sockaddr_in from;
    struct sockaddr_in addr;
    socklen_t fromlen;
    socklen_t length;
    char ThisHost[80];

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    gethostname(ThisHost, MAXHOSTNAME);

    printf("----TCP/Client running at host NAME: %s\n", ThisHost);
    if ((hp = gethostbyname(ThisHost)) == NULL)
    {
        fprintf(stderr, "Can't find host %s\n", argv[1]);
        exit(-1);
    }
    bcopy(hp->h_addr, &(server.sin_addr), hp->h_length);
    printf("    (TCP/Client INET ADDRESS is: %s )\n", inet_ntoa(server.sin_addr));

    if ((hp = gethostbyname(argv[1])) == NULL)
    {
        addr.sin_addr.s_addr = inet_addr(argv[1]);
        if ((hp = gethostbyaddr((char *)&addr.sin_addr.s_addr,
                                sizeof(addr.sin_addr.s_addr), AF_INET)) == NULL)
        {
            fprintf(stderr, "Can't find host %s\n", argv[1]);
            exit(-1);
        }
    }
    printf("----TCP/Server running at host NAME: %s\n", hp->h_name);
    bcopy(hp->h_addr, &(server.sin_addr), hp->h_length);
    printf("    (TCP/Server INET ADDRESS is: %s )\n", inet_ntoa(server.sin_addr));

    server.sin_family = AF_INET;

    server.sin_port = htons(7878);

    sd = socket(AF_INET, SOCK_STREAM, 0);

    if (sd < 0)
    {
        perror("opening stream socket");
        exit(-1);
    }

    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        close(sd);
        perror("connecting stream socket");
        exit(0);
    }
    fromlen = sizeof(from);
    if (getpeername(sd, (struct sockaddr *)&from, &fromlen) < 0)
    {
        perror("could't get peername\n");
        exit(1);
    }
    printf("Connected to TCPServer1: ");
    printf("%s:%d\n", inet_ntoa(from.sin_addr),
           ntohs(from.sin_port));
    if ((hp = gethostbyaddr((char *)&from.sin_addr.s_addr,
                            sizeof(from.sin_addr.s_addr), AF_INET)) == NULL)
        fprintf(stderr, "Can't find host %s\n", inet_ntoa(from.sin_addr));
    else
        printf("(Name is : %s)\n", hp->h_name);
    childpid = fork();
    if (childpid == 0)
    {
        if (signal(SIGINT, sig_int) == SIG_ERR)
            printf("signal(SIGINT) error");
        if (signal(SIGTSTP, sig_tstp) == SIG_ERR)
            printf("signal(SIGTSTP) error");
        GetUserInput();
    }

    for (;;)
    {
        cleanup(rbuf);
        if ((rc = recv(sd, rbuf, sizeof(buf), 0)) < 0)
        {
            perror("receiving stream  message");
            exit(-1);
        }
        if (rc > 0)
        {
            rbuf[rc] = '\0';
            printf("%s", rbuf);
        }
        else
        {
            printf("Disconnected..\n");
            close(sd);
            exit(0);
        }
    }
}

void cleanup(char *buf)
{
    int i;
    for (i = 0; i < BUFSIZE; i++)
        buf[i] = '\0';
}

char *updateCommands(char *inString, char *cmdType)
{

    char *finalCmd = (char *)malloc(sizeof(char) * 1000);
    if (inString != NULL)
    {
        strcat(finalCmd, cmdType);
        strcat(finalCmd, " ");
        strcat(finalCmd, inString);
    }
    else
    {
        return NULL;
    }
    return finalCmd;
}

char *removeGarbage(char *inS)
{
    char *savePtr;
    const char s[4] = "\t\n";
    char *newCmd = strtok_r(inS, s, &savePtr);
    return newCmd;
}

void GetUserInput()
{
    char *inString = (char *)malloc(sizeof(char) * 1000);
    char *inputCommand = (char *)malloc(sizeof(char) * 1000);
    char *cmdString;
    while ((inString = readline("")))
    {
        inputCommand = removeGarbage(inString);
        if (inputCommand != NULL)
        {
            cmdString = updateCommands(inputCommand, "CMD");
            if (send(sd, cmdString, strlen(cmdString), 0) < 0)
                perror("sending stream message");
        }
    }
    printf("EOF... exit\n");
    close(sd);
    kill(getppid(), 9);
    exit(0);
}
