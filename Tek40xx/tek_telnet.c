/* tek_telnet.c
 *
Copyright (c) 2015 Mittorn.
Copyright (c) 2018, Ian Schofield

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the author shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the author.
*/

/**************************************************************************
*   This is a simple telnet chat utility. It connects to server and
*	sends/reads lines. Useful for some routers configuration.
*	based on simple-client.c by Sean Walton and Macmillan Publishers
***************************************************************************

***************************************************************************
*   Modified by Ian Schofield April 2019
***************************************************************************
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit()

#if defined (__linux__) || defined (VMS) || defined (__APPLE__)
#include <errno.h> // errno will not work on windows
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#ifdef _WIN32
#include <winsock.h>
#pragma comment(lib,"wsock32.lib") //Winsock Library
#define close closesocket
#define errexit 1
#define perror(x) fprintf(stderr, "%s: failed\n", x)
#endif

#define PORT_INT		23			  // telnet connection port
#define SERVER_ADDR	 "192.168.1.195"
#define MAXBUF		  1024

#define bzero(x,y) memset(x,0,y)

#define DO 0xfd
#define WONT 0xfc
#define WILL 0xfb
#define DONT 0xfe
#define CMD 0xff
#define CMD_ECHO 1
#define CMD_WINDOW_SIZE 31

extern char tline[128];

void negotiate(int sock, unsigned char *buf, int len)
{
    int i;
    unsigned char tmp2[10] = {255, 250, 31, 0, 80, 0, 24, 255, 240};
    unsigned char tmp1[10] = {255, 251, 31};

    if (buf[1] == DO && buf[2] == CMD_WINDOW_SIZE)
    {
        if (send(sock, tmp1, 3 , 0) < 0)
            exit(1);

        if (send(sock, tmp2, 9, 0) < 0)
            exit(1);
        return;
    }

    for (i = 0; i < len; i++)
    {
        if (buf[i] == DO)
            buf[i] = WONT;
        else if (buf[i] == WILL)
            buf[i] = DO;
    }

    if (send(sock, buf, len , 0) < 0)
        exit(1);
}

#ifdef _WIN32
static int inet_aton(const char *cp, struct in_addr *inp)
{
    unsigned long addr;
    if (cp == 0 || inp == 0)
    {
        return -1;
    }

    addr = inet_addr(cp);
    if (addr == INADDR_NONE || addr == INADDR_ANY)
    {
        return -1;
    }

    inp->s_addr = addr;
    return -1;
}
#endif

int hostname_to_ip(char * hostname , char* ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;

    if ( (he = gethostbyname( hostname ) ) == NULL)
    {
        // get the host info
        perror("Host name lookup: ");
        return 1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;

    for(i = 0; addr_list[i] != NULL; i++)
    {
        //Return the first one;
        strcpy((char *)ip , (const char *)(inet_ntoa(*addr_list[i])) );
        return 0;
    }

    return 1;
}

int sockfd;

// send string
void sendstring(const char *s)
{
    int len = strlen(s);
    // Only small messages supported now,
    // need to improve this
    send(sockfd, s, len, 0);
}

/* Start telnet transactions
*  return first received data string and sockfd
*/

int telnet(char *hostname, int port)
{
    struct sockaddr_in dest;
    long rc = 0;
    unsigned char buf[256];
    char ipaddr[128];

#ifdef _WIN32
    static WSADATA winsockdata;
    WSAStartup( MAKEWORD( 1, 1 ), &winsockdata );
#endif

    // Open socket
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        perror("Socket creation failed");
        exit(errno);
    }

    // Initialize server address/port struct
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    hostname_to_ip(hostname,ipaddr);
    if ( inet_aton(ipaddr, (struct in_addr *)&dest.sin_addr.s_addr) == 0 )
    {
        perror("Cannot resolve requested host\r\n");
        exit(errno);
    }
#if 0 // if you wish to enable non-blocking mode, enable this
#ifdef WIN32
    unsigned long arg = 1;
    ioctlsocket(sockfd, FIONBIO, &arg);
#else
    int arg = fcntl(sockfd, F_GETFL, NULL);
    arg |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, arg);
#endif
#endif
    // Connect to server
    if ( connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) != 0 )
    {
        perror("connect");
        exit(errno);
    }

    // Telnet negociation break on data packet
    while(1)
    {
        memset(buf,0,sizeof(buf));
        rc=recv(sockfd,buf,256,0);
        if (*buf != 255)
            break;
        negotiate(sockfd,buf,rc);
    }
    strcpy(tline,(const char *)buf);
    return sockfd;
}
