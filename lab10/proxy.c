/*
 * proxy.c - ICS Web proxy
 *
 *
 */

#include <stdarg.h>
#include <sys/select.h>

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char* uri, char* target_addr, char* path, char* port);
void format_log_entry(char* logstring, struct sockaddr_in* sockaddr, char* uri,
                      size_t size);
void* thread_routine(void* thread_arg);
size_t proxy_receive(int connfd, rio_t* client_rio);

// Rio 库的错误检查包装函数。
ssize_t Rio_readnb_w(rio_t* rp, void* usrbuf, size_t n) {
  // rc 用于存储读取的字节数。
  ssize_t rc;
  //从 rio_t 结构体中读取 n 字节数据到 usrbuf。
  if ((rc = rio_readnb(rp, usrbuf, n)) < 0) {
    //如果读取失败，打印错误信息并返回 0。
    fprintf(stderr, "Rio_readnb error\n");
    return 0;
  }
  //返回读取的字节数。
  return rc;
}

// Rio 库的错误检查包装函数。
ssize_t Rio_readlineb_w(rio_t* rp, void* usrbuf, size_t maxlen) {
  // rc 用于存储读取的字节数。
  ssize_t rc;
  //从 rio_t 结构体中读取一行数据到 usrbuf。
  if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
    //如果读取失败，打印错误信息并返回 0。
    fprintf(stderr, "Rio_readlineb error\n");
    return 0;
  }
  //返回读取的字节数。
  return rc;
}

// Rio 库的错误检查包装函数。
int Rio_writen_w(int fd, void* usrbuf, size_t n) {
  //如果写入失败，打印错误信息并返回 -1。
  if (rio_writen(fd, usrbuf, n) != n) {
    //如果写入失败，打印错误信息并返回 -1。
    fprintf(stderr, "Rio_writen error\n");
    return -1;
  }
  return 0;
}

/*
 * Global variable
 */
sem_t log_mutex;

/*
 * Struct for thread arguments
 */
struct arg_t {
  int connfd;
  struct sockaddr_in clientaddr;
};

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char** argv) {
  int listenfd;
  socklen_t clientlen;

  initialize_server(argc, argv, &listenfd, &clientlen);

  while (1) {
    accept_client_connection(listenfd, clientlen);
  }
}

// Initialize server
void initialize_server(int argc, char** argv, int* listenfd, socklen_t* clientlen) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
    exit(0);
  }

  *listenfd = Open_listenfd(argv[1]);
  *clientlen = sizeof(struct sockaddr_in);
  Sem_init(&log_mutex, 0, 1);
  Signal(SIGPIPE, SIG_IGN);
}

// Accept client connection and create a new thread to handle it
void accept_client_connection(int listenfd, socklen_t clientlen) {
  pthread_t tid;
  struct arg_t* thread_arg = Malloc(sizeof(struct arg_t));
  struct sockaddr* addr = (SA*)&(thread_arg->clientaddr);
  thread_arg->connfd = Accept(listenfd, addr, &clientlen);
  Pthread_create(&tid, NULL, &thread_routine, thread_arg);
}

//这是一个线程例程，所有新创建的线程都会运行这个函数
void* thread_routine(void* thread_arg) {
  // Pthread_detach 用于将当前线程设置为分离状态。
  //分离状态的线程在结束时会自动释放资源，而无需其他线程调用 pthread_join
  Pthread_detach(Pthread_self());

  //将传递给线程的参数 thread_arg 强制转换为 arg_t 结构体指针类型
  struct arg_t* arg_self = (struct arg_t*)thread_arg;
  // proxy_manager 是代理服务器的主要处理函数，负责管理请求和响应。
  // proxy_manager 函数使用 arg_self->connfd（客户端连接文件描述符）和
  // arg_self->clientaddr（客户端地址）进行处理 proxy_manager 函数：
  // proxy_manager 是代理服务器的主要处理函数，负责处理客户端的请求和响应。
  //函数原型为 void proxy_manager(int connfd, struct sockaddr_in* clientaddr)。
  //参数传递：
  // arg_self->connfd：访问 arg_self 结构体中的 connfd 成员，这个成员是一个 int
  // 类型，代表客户端连接的文件描述符。该文件描述符用于与客户端通信。
  //&(arg_self->clientaddr)：访问 arg_self 结构体中的 clientaddr
  //成员，并获取它的地址。clientaddr 是一个 struct sockaddr_in
  //类型，存储客户端的地址信息。传递地址是因为 proxy_manager 函数需要一个指向
  // struct sockaddr_in 的指针。
  proxy_manager(arg_self->connfd, &(arg_self->clientaddr));

  //关闭与客户端的连接。
  Close(arg_self->connfd);
  //释放为 arg_self 动态分配的内存，避免内存泄漏
  Free(arg_self);
  //返回 NULL，表示线程已结束
  return NULL;
}

//解析请求行
int parse_request_line(rio_t* conn_rio, char* method, char* uri, char* version) {
  char buf[MAXLINE];
  if (!Rio_readlineb_w(conn_rio, buf, MAXLINE)) {
    fprintf(stderr, "error: read empty request line\n");
    return -1;
  }
  if (sscanf(buf, "%s %s %s", method, uri, version) < 3) {
    fprintf(stderr, "error: mismatched parameters\n");
    return -1;
  }
  return 0;
}

//读取请求头
size_t read_request_headers(rio_t* conn_rio, char* req_header, size_t* content_length) {
  char buf[MAXLINE];
  char tmp_header[MAXLINE];
  size_t n = Rio_readlineb_w(conn_rio, buf, MAXLINE);
  while (n != 0) {
    if (strncasecmp(buf, "Content-Length", 14) == 0) {
      sscanf(buf + 15, "%zu", content_length);
    }
    strcpy(tmp_header, req_header);
    sprintf(req_header, "%s%s", tmp_header, buf);
    if (strncmp(buf, "\r\n", 2) == 0) break;
    n = Rio_readlineb_w(conn_rio, buf, MAXLINE);
  }
  return n;
}

//连接到目标服务器
int connect_to_server(char* hostname, char* port, rio_t* client_rio) {
  int clientfd = open_clientfd(hostname, port);
  if (clientfd < 0) {
    fprintf(stderr, "error: open client fd error (hostname: %s, port: %s)\n", hostname, port);
    return -1;
  }
  Rio_readinitb(client_rio, clientfd);
  return clientfd;
}

//记录日志
void log_request(struct sockaddr_in* clientaddr, char* uri, size_t byte_size) {
  char buf[MAXLINE];
  format_log_entry(buf, clientaddr, uri, byte_size);
  P(&log_mutex);
  printf("%s\n", buf);
  V(&log_mutex);
}

//代理服务器的主要处理函数，负责管理请求和响应。
void proxy_manager(int connfd, struct sockaddr_in* clientaddr) {
  char method[MAXLINE / 4], uri[MAXLINE], version[MAXLINE / 2];
  char hostname[MAXLINE], pathname[MAXLINE], port[MAXLINE];
  char req_header[MAXLINE * 2] = {0};
  rio_t conn_rio, client_rio;
  size_t byte_size = 0, content_length = 0;

  Rio_readinitb(&conn_rio, connfd);

  if (parse_request_line(&conn_rio, method, uri, version) != 0) return;
  if (parse_uri(uri, hostname, pathname, port) != 0) {
    fprintf(stderr, "error: parse uri error\n");
    return;
  }

  sprintf(req_header, "%s /%s %s\r\n", method, pathname, version);
  if (read_request_headers(&conn_rio, req_header, &content_length) == 0) return;

  int clientfd = connect_to_server(hostname, port, &client_rio);
  if (clientfd < 0) return;

  if (proxy_send(clientfd, &conn_rio, req_header, content_length, method) == 0) {
    byte_size = proxy_receive(connfd, &client_rio);
  }

  log_request(clientaddr, uri, byte_size);
  Close(clientfd);
}

//发送请求到服务器。
int proxy_send(int clientfd, rio_t* conn_rio, char* req_header, size_t length,
               char* method) {
  // buf 用于存储数据。
  char buf[MAXLINE];

  //写入请求头。
  if (Rio_writen_w(clientfd, req_header, strlen(req_header)) == -1) {
    return -1;
  }
  //如果方法不是 GET，则写入请求体。
  if (strcasecmp(method, "GET") != 0) {
    //循环写入请求体。
    for (int i = 0; i < length; ++i) {
      //从 conn_rio 中读取一字节数据到 buf。
      if (Rio_readnb_w(conn_rio, buf, 1) == 0) return -1;
      //将 buf 写入到 clientfd。
      if (Rio_writen_w(clientfd, buf, 1) == -1) return -1;
    }
  }

  return 0;
}

//读取响应头
size_t read_response_headers(rio_t* client_rio, int connfd, size_t* content_length) {
  char buf[MAXLINE];
  size_t byte_size = 0;
  size_t n = Rio_readlineb_w(client_rio, buf, MAXLINE);

  while (n != 0) {
    byte_size += n;
    if (strncasecmp(buf, "Content-Length", 14) == 0) {
      sscanf(buf + 15, "%zu", content_length);
    }
    if (Rio_writen_w(connfd, buf, strlen(buf)) == -1) {
      return 0;
    }
    if (strncmp(buf, "\r\n", 2) == 0) break;
    n = Rio_readlineb_w(client_rio, buf, MAXLINE);
  }

  return byte_size;
}

//读取响应体
size_t read_response_body(rio_t* client_rio, int connfd, size_t content_length) {
  char buf[MAXLINE];
  size_t byte_size = 0;

  for (size_t i = 0; i < content_length; ++i) {
    if (Rio_readnb_w(client_rio, buf, 1) == 0) return 0;
    if (Rio_writen_w(connfd, buf, 1) == -1) return 0;
    ++byte_size;
  }

  return byte_size;
}

//接收响应
size_t proxy_receive(int connfd, rio_t* client_rio) {
  size_t content_length = 0;
  size_t byte_size = read_response_headers(client_rio, connfd, &content_length);

  if (byte_size == 0) return 0;

  byte_size += read_response_body(client_rio, connfd, content_length);

  return byte_size;
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char* uri, char* hostname, char* pathname, char* port) {
  char* hostbegin;
  char* hostend;
  char* pathbegin;
  int len;

  if (strncasecmp(uri, "http://", 7) != 0) {
    hostname[0] = '\0';
    return -1;
  }

  /* Extract the host name */
  hostbegin = uri + 7;
  hostend = strpbrk(hostbegin, " :/\r\n\0");
  if (hostend == NULL) return -1;
  len = hostend - hostbegin;
  strncpy(hostname, hostbegin, len);
  hostname[len] = '\0';

  /* Extract the port number */
  if (*hostend == ':') {
    char* p = hostend + 1;
    while (isdigit(*p)) *port++ = *p++;
    *port = '\0';
  } else {
    strcpy(port, "80");
  }

  /* Extract the path */
  pathbegin = strchr(hostbegin, '/');
  if (pathbegin == NULL) {
    pathname[0] = '\0';
  } else {
    pathbegin++;
    strcpy(pathname, pathbegin);
  }

  return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char* logstring, struct sockaddr_in* sockaddr, char* uri,
                      size_t size) {
  time_t now;
  char time_str[MAXLINE];
  char host[INET_ADDRSTRLEN];

  /* Get a formatted time string */
  now = time(NULL);
  strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

  if (inet_ntop(AF_INET, &sockaddr->sin_addr, host, sizeof(host)) == NULL)
    unix_error("Convert sockaddr_in to string representation failed\n");

  /* Return the formatted log entry string */
  sprintf(logstring, "%s: %s %s %zu", time_str, host, uri, size);
}

