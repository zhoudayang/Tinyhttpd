//这是一个简单的web服务器
//实现了web服务器的get方法
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
//判断是否是空格
#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
//接收请求
void accept_request(void *);
//发送文件
void cat(int, FILE *);
//显示错误
void error_die(const char *);
//执行cgi程序
//从客户端读取请求
int get_line(int, char *, int);
//发送header
void headers(int, const char *);
//没有找到文件
void not_found(int);
//发送文件
void serve_file(int, const char *);
//服务器初始化
int startup(u_short *);
// 未实现的方法
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(void *arg)
{
    //获取参数  client 对应的文件读写锚点
    int client = *(int*)arg;
    char buf[1024];
    size_t numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI
                       * program */
    char *query_string = NULL;

    numchars = get_line(client, buf, sizeof(buf));
    i = 0; j = 0;
    /*
     * 下面是 buf中的内容示例
     * GET / HTTP/1.1
     * GET /index.html HTTP/1.1
     */
    //buf[i]非空,且数组method不越界
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j=i;
    method[i] = '\0';
    //如果既不是GET又不是POST方法,直接返回
    //因为没有实现除了GET和POST之外的方法
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        //返回 出错信息
        unimplemented(client);
        return;
    }
    //如果是POST方法
    if (strcasecmp(method, "POST") == 0) {
        //返回 出错信息
        unimplemented(client);
        return;
    }
    //跳过空格
    i = 0;
    while (ISspace(buf[j]) && (j < numchars))
        j++;
    //获取url
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';
    //读取的文件路径为服务器程序所在目录下的htdocs文件夹中
    sprintf(path, "htdocs%s", url);
    //如果url是/ 返回index.html
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    //如果没有找到文件
    if (stat(path, &st) == -1) {
        //丢弃 无意义的字符
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));

        //没有找到请求的资源
        not_found(client);
    }
    else
    {
        //如果是目录,定位到该目录下的index.html文件
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        serve_file(client, path);
    }
    close(client);
}


/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
//向客户端发送文件,采用缓冲区读取文件,并将缓冲区中读取的文件内容发送到客户端
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}


/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error.
 * 错误处理函数,输出错误,并且中断程序执行
 * */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}


/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/* 向客户端发送信息,没有找到相关资源
 * */
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    //没有找到文件
    if (resource == NULL)
        not_found(client);
    else
    {
        //向客户端发送header信息
        headers(client, filename);
        //向客户端发送文件
        cat(client, resource);
    }
    //关闭文件
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    //ipv4地址
    name.sin_family = AF_INET;
    //端口
    name.sin_port = htons(*port);
    //对一个服务器上的所有网卡的多个本地ip地址都绑定端口号进行监听
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    //绑定端口
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    //随机绑定端口号
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);
        //随机获取一个sockaddr结构体实例
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        //获取结构体中的端口号
        *port = ntohs(name.sin_port);
    }
    /*
    DESCRIPTION         top

    listen() marks the socket referred to by sockfd as a passive socket,
    that is, as a socket that will be used to accept incoming connection
    requests using accept(2).

        The sockfd argument is a file descriptor that refers to a socket of
    type SOCK_STREAM or SOCK_SEQPACKET.

        The backlog argument defines the maximum length to which the queue of
    pending connections for sockfd may grow.  If a connection request
    arrives when the queue is full, the client may receive an error with
    an indication of ECONNREFUSED or, if the underlying protocol supports
    retransmission, the request may be ignored so that a later reattempt
    at connection succeeds.
        RETURN VALUE         top

    On success, zero is returned.  On error, -1 is returned, and errno is
    set appropriately.
    */
    //开始监听,允许的等待连接的队列长度设置为5
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket
 * 向客户端发送信息,没有实现的http方法
 * */
/**********************************************************************/
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;
    u_short port = 8888;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t  client_name_len = sizeof(client_name);
    pthread_t newthread;

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        client_sock = accept(server_sock,
                             (struct sockaddr *)&client_name,
                             &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        /* accept_request(client_sock); */
        //创建线程用于处理客户端发起的访问请求
        //调用函数accept_request
        //传入的参数为sockaddr_in 类型的结构体
        if (pthread_create(&newthread , NULL, (void *)accept_request, (void *)&client_sock) != 0)
            perror("pthread_create");
    }
    close(server_sock);

    return(0);
}