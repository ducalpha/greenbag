// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>     // isspace()
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h> // the following 3 includes are for stat()
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include "ahttpd.h"
#include "gbsession.h"
#include "util.h"
#include "extra_info.h"
#include "color.h"
#define MULTI_THREADED
#define BUF_FILE_BUF_SIZE 128 // if too big, send will fail
#define SERVER_STRING "Server: ahttpd/0.1.0"
#define INDEX_PAGE "index.html"
#define DEFAULT_TYPE_IDX 0
#define GET "GET"
#define POST "POST"
#define TIMEOUT_WAIT_COUNT 100 // 10s
#define DEFAULT_RESPONSE_HTTP_VERSION "HTTP/1.0"
#define HTTP_SLASH "http://"
#define OK_CODE_MULTI_THREADED 0
//Note: Make sure these arrays are synced
char *file_exts[] = {"html", "mp4", "flv", "ico"};
char *mime_types[] = {"text/html", "video/mp4", "video/x-flv", "image/x-icon"};
int gproxy_stop = 0;
int ahttpd_argc = 0;
char **ahttpd_argv = NULL;
int run_once_mode = 0;

void *gb_thread_main(void *arg);
int startup(unsigned int *);
void *accept_request(void *);
int get_line(int sock, char *buf, int size);
void unimplemented(int client_sock, char *http_version);
void skip_headers(int client_sock);
void not_found(int client_sock, const char *http_version);
void send_file(int client_sock, const char *filepath, const char *http_version);
void send_remote_file(int client_sock, const char *url, const char *http_version);
void send_local_file(int client_sock, const char *filepath, const char *http_version);
void send_file_headers(int client_sock, const char *filename, const char *http_version, long long int content_length);
void send_file_content(int client_socket, const char *filepath);
int is_url_to_our_server(const char *url);
void parse_ahttpd_args(int argc, char **argv);
int server_sock = -1;

int main(int argc, char** argv)
{
#ifdef MULTI_THREADED
  pthread_t process_request_thread; //unused
#endif
  unsigned int server_port = GPROXY_PORT;
  int client_sock = -1;
  struct sockaddr_in client_addr;
  ahttpd_argc = argc; ahttpd_argv = argv;
  parse_ahttpd_args(argc, argv);
  /* start the internal web server */
  socklen_t client_addr_len = sizeof(client_addr);
  server_sock = startup(&server_port);
  printf("httpd running on port %u\n", server_port);
  while (!gproxy_stop)
  {
    printf("%c[%d;%d;%dmGproxy is waiting for a request%c[%d;%dm\n", 
           0x1B, BRIGHT, YELLOW, BG_BLACK, 0x1B, WHITE, ATTR_OFF);
    if ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len)) == -1) {
      perror("Socket accept error");
      if (run_once_mode)
        break;
      continue;
    }
#ifndef NDEBUG
    fprintf(stderr, "Accepted a request\n");
#endif
#ifndef MULTI_THREADED
    accept_request((void*)&client_sock);
    if (run_once_mode)
      gproxy_stop = 1;
#else
    Pthread_create(&process_request_thread, NULL, accept_request, (void *)&client_sock);
#endif
  }
  shutdown(server_sock, SHUT_RDWR);
  close(server_sock);
  printf("%c[%d;%d;%dmGproxy finishes successfully%c[%d;%dm\n",
         0x1B, BRIGHT, GREEN, BG_BLACK, 0x1B, WHITE, ATTR_OFF);
  return 0;
}

void *accept_request(void *cln_sock)
{
  int client_sock = *((int *)cln_sock);
  char http_line[2048]; // = method + space + url + space + http_version
  char method[16]; // no method among the 9 is more than 16 chars long
  char url[2023];
  char http_version[9]; // HTTP/1.1 (8chars) + NULL
  char *remote_url = NULL;
  int numchars = 0;
  int i,j;
/*******Extract information from the first HTTP line************/
  numchars = get_line(client_sock, http_line, sizeof(http_line));
  for(i = 0, j = 0; !IS_SPACE(http_line[j]); ++i, ++j) {
    method[i] = http_line[j];
    if ((i >= sizeof(method)) || (j >= sizeof(http_line)))
      error_die("Method string is too long\n");
  }
  method[i] = '\0';
  while (IS_SPACE(http_line[j])) {
    ++j;
    if (j >= sizeof(http_line))
      error_die("Nothing after the method\n");
  }
  for (i = 0; !IS_SPACE(http_line[j]); ++i, ++j) {
    url[i] = http_line[j];
    if ((i >= sizeof(url)) || (j >= sizeof(http_line)))
      error_die("URL string is too long\n");
  }
  url[i] = '\0';
  while (IS_SPACE(http_line[j])) {
    ++j;
    if (j >= sizeof(http_line))
      error_die("Nothing after the URL\n");
  }
  for (i = 0; i < 8; ++i, ++j) {
    http_version[i] = http_line[j];
    if (j >= sizeof(http_line))
      error_die("Index out of bound\n");
  }
  http_version[i] = '\0';
#ifndef NDEBUG
//  printf("Method is %s\n", method);
//  printf("URL is %s\n", url);
//  printf("HTTP version is %s\n", http_version);
#endif
/**************************************************************/
  // only support GET method
  if (strcasecmp(method, GET))
  {
    unimplemented(client_sock, http_version);
    return NULL;
  }

  if (check_url_remote(url))
  {
    remote_url = extract_remote_url(url);
    char *protocol, *host, *port, *abs_path;
    char local_path[512];
    struct stat file_status;
    decompose_url(remote_url, &protocol, &host, &port, &abs_path);
    strcpy(local_path, ".");
    strcat(local_path, strrchr(abs_path, '/')); //copy the "/" also
    printf("remote_url:%s\tabs_path:%s\tlocal path: %s\n", remote_url, abs_path, local_path);
    // if the file exist --> send_local_file
    if (file_exists(local_path, &file_status))
    {
      printf("Local file exists, send the local file\n");
      send_local_file(client_sock, abs_path, http_version);
    }
    else
    {
      send_remote_file(client_sock, remote_url, http_version);
    }
    if (protocol)  free(protocol);
    if (host) free(host);
    if (port) free(port);
    if (abs_path) free(abs_path);
  }
  else
  {
    send_local_file(client_sock, url, http_version);
  }
  
  //close(client_sock);
  shutdown(client_sock, SHUT_WR);

#ifdef MULTI_THREADED
  if (run_once_mode)
  {
    shutdown(server_sock, SHUT_RDWR);
    close(server_sock);
    //exit(1);
  }
#endif

  return NULL;
}

/*
 * Get a line from a socket.
 * Return number of characters read
 * Output a buffer that is null-terminated
 */
int get_line(int sock, char *buf, int buf_size)
{
  int buf_idx = 0;
  char c = '\0';
  int num_rcv_char = 0;
  int read_cr = 0;
  int read_crlf = 0;
  const int limit = 1000;
  int count = 0;

  while (!read_crlf)
  {
    num_rcv_char = recv(sock, &c, 1, 0);  // blocking operation
    if (num_rcv_char <= 0)
    {
      perror("recv, get_line; connection gone"); //from tinyproxy
      break;
    }
#ifndef NDEBUG
    //printf("%X", c);
    printf("%c", c);
#endif
    if (num_rcv_char > 0)
    {
      buf[buf_idx++] = c;
      if (read_cr && c == '\n')
        read_crlf = 1;
      if (c == '\r')
        read_cr = 1;
      else
        read_cr = 0;
      assert(buf_idx < buf_size); //"Buffer overflows\n" TODO: allocate more memory
    }
    if (count++ >= limit)
    {
      fprintf(stderr, "infinite loop\n");
      close(sock);
      //exit(2);
      break;
    }
  }
  buf[buf_idx] = '\0';
  return (buf_idx - 1);
}

/**
 * Create a server that listens to the provided port
 * If the port is 0, assign any free port
 * Return the file descriptor of the socket
 * Return the port if it is dynamically allocated
*/
int startup(unsigned int *port)
{
  int httpd = 0;
  struct sockaddr_in server_addr;
  int optval;
  httpd = Socket(PF_INET, SOCK_STREAM, 0);
  Setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(*port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  //server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  Bind(httpd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (*port == 0) // bind to any free port
  {
    socklen_t server_addr_len = sizeof(server_addr);
    Getsockname(httpd, (struct sockaddr *)&server_addr, &server_addr_len);
    *port = ntohs(server_addr.sin_port);
  }
  Listen(httpd, 5); // listen queue is 5
  return httpd;
}

/*
 * retrieve and send a remote file to client
 */
void send_remote_file(int client_socket, const char *url, const char *http_version)
{
  pthread_t gb_thread;
  char *return_value;
  int wait_count = 0;
  int buf_file = 0;
  char buf_file_buf[BUF_FILE_BUF_SIZE];
  int read_size, nread_bytes, nsent_bytes;
  off_t first_not_sent_byte;
  skip_headers(client_socket);
  if (pthread_create(&gb_thread, NULL, (void *)gb_thread_main, strdup(url)) != OK_CODE_MULTI_THREADED)
    perror_die("pthread_create gb_thread_main");
  // wait until downloaded file & there is some data downloaded
  while ((gb == NULL) || (gb->filename == NULL) || (gb->last_downloaded_byte <= 0)) {
    usleep(100*1000); // wait 100ms
    fprintf(stderr, "Waiting for gb_process to download some data\n");
    if (wait_count++ >= TIMEOUT_WAIT_COUNT) // time out! or something wrong
    {
      fprintf(stderr, "Time out, gb_process downloaded nothing, return\n");
      not_found(client_socket, http_version);
      return;
    }
  }
#ifndef NDEBUG
  fprintf(stderr, "url: %s\n", url);
  fprintf(stderr, "buffer file path: %s\n", gb->filename);
#endif
  if ((buf_file = open(gb->filename, O_RDONLY)) == -1)
    perror_die("open buffer file");
  send_file_headers(client_socket, gb->filename, http_version, gb->size);
  first_not_sent_byte = 0;
  while(1)
  { // read file - send - update stat
    if (first_not_sent_byte <= gb->last_downloaded_byte)
    {
      read_size = (sizeof(buf_file_buf) <= (gb->last_downloaded_byte - first_not_sent_byte))?
                  sizeof(buf_file_buf):
                  (gb->last_downloaded_byte - first_not_sent_byte);
      nread_bytes = read(buf_file, buf_file_buf, read_size);
      // send the bytes read to the client
      if (nread_bytes > 0)
      { 
send_again:        nsent_bytes = send(client_socket, buf_file_buf, nread_bytes, MSG_NOSIGNAL);
#ifndef NDEBUG
//        fprintf(stderr, "sending buffer: %s\n", buf_file_buf);
#endif
        if (nsent_bytes <= 0) 
        {
          if (errno == EINTR) {
            fprintf(stderr, "\nsend, send_remote_file, is interrupted by a signal");
            goto send_again;
          } else {
            perror("\nsend, send_remote_file, connection gone");
            break;
          }
        } else {
          nsent_bytes_to_client += nsent_bytes;
        }
      }
      if (first_not_sent_byte == gb->size)  //finished
        break;
      if ((first_not_sent_byte = lseek(buf_file, 0, SEEK_CUR)) < 0)
      {
        perror("WARNING: [ahttp]fail to seek to the first not sent byte");
        break;
      }
#ifndef NDEBUG
//    fprintf(stderr, "sent bytes: %d\tfirst_not_sent_byte: %lld\tlast_downloaded_byte: %lld\n", nsent_bytes, (long long int)first_not_sent_byte, gb->last_downloaded_byte);
 //     if ((first_not_sent_byte % 100000) == 0)
   //if (read_size > 0)
   // fprintf(stderr, "first_not_sent_byte: %ld\tlast_downloaded_byte: %lld\n", first_not_sent_byte, gb->last_downloaded_byte);
#endif
      assert(first_not_sent_byte <= gb->last_downloaded_byte);
      assert(first_not_sent_byte <= gb->size);
    }
    else 
    { // wait for a moment for new data
      usleep(1*1000); //wait 1ms
    }
  }
  close(buf_file);
  assert(gb);
  char *buf_filename = strdup(gb->filename);
  gb->run = 0;
  gb->finish = 1;
  printf("Waiting for gb_process to finish\n");
  if (pthread_join(gb_thread, (void *)&return_value) != OK_CODE_MULTI_THREADED)
    perror("WARNING: [ahttpd]joining with gb_thread failed");
  printf("Joined with gb_process\n");
  if (unlink(buf_filename) == -1) {
    fprintf(stderr, "WARNING: unlink %s failed\n", buf_filename);
    //exit(5);
  }
  free(buf_filename);
}

/*
 * send a local file to client
 * url example: /TurkishAirlines.mp4
 */
void send_local_file(int client_sock, const char *url, const char *http_version)
{
  char local_path[512];
  struct stat file_status;
  //asprintf(&local_path, "htdocs%s", url); // local_path ends with '\0'
  strcpy(local_path, ".");
  strcat(local_path, url); // local_path ends with '\0'
  // URL ends with '/' --> appends index.html or something
  if (local_path[strlen(local_path) - 1] == '/')
    strcat(local_path, INDEX_PAGE);
  if ((file_status.st_mode & S_IFMT) == S_IFDIR) //directory
  {
    strcat(local_path, PATH_SEPARATOR_STR); // append "/"
    strcat(local_path, INDEX_PAGE); // strcat appends '\0'
  }
  if (stat(local_path, &file_status) == -1)
  {
#ifndef NDEBUG
    perror("stat");
#endif
    skip_headers(client_sock);
    not_found(client_sock, http_version);
  }
  else
  {
    send_file(client_sock, local_path, http_version);
  }
}
/*
 * Send a regular file to the client
 */
void send_file(int client_sock, const char *filepath, const char *http_version)
{
  FILE *fp = NULL;
  const char *filename = NULL ;
  skip_headers(client_sock);
  fp = fopen(filepath, "r");
  if (fp == NULL)
  {
#ifndef NDEBUG
    perror("fopen, send_file()");
#endif
    not_found(client_sock, http_version);
  }
  else
  {
    fclose(fp);
    if ((filename = strrchr(filepath, PATH_SEPARATOR)) == NULL)
      filename = filepath;
    send_file_headers(client_sock, filename, http_version, 0); // TODO get file size and replace 0 by it
    send_file_content(client_sock, filepath);
  }
}

/*
 * send the informational HTTP headers about a file
 */
void send_file_headers(int client_sock, const char *filename, const char *http_version, long long int content_length)
{
  char response_headers[1024];
  char *content_type;
  int i;
  char *file_ext = strrchr(filename, EXT_SEPARATOR); 
  memset(response_headers, 0, sizeof(response_headers));
  if (file_ext == NULL)
  {
    content_type = mime_types[DEFAULT_TYPE_IDX];
  }
  else
  {
    ++file_ext;//exentsion separator position
    for (i = 0; i < sizeof(file_exts)/sizeof(*file_exts); ++i)
    {
      if (!strcmp(file_ext,file_exts[i]))
        break;
    }
    assert(sizeof(file_exts)/sizeof(*file_exts) == sizeof(mime_types)/sizeof(*mime_types));
    if (i < sizeof(file_exts)/sizeof(*file_exts))
      content_type = mime_types[i];
    else
      content_type = mime_types[DEFAULT_TYPE_IDX];
  }
  sprintf(response_headers,
"%s 200 OK\r\n\
%s\r\n\
Accept-Ranges: none\r\n\
Connection: close\r\n\
Content-Type: %s\r\n", DEFAULT_RESPONSE_HTTP_VERSION, SERVER_STRING, content_type);
  if (content_length > 0)
    sprintf(response_headers + strlen(response_headers), "Content-Length: %lld\r\n", content_length);
  sprintf(response_headers + strlen(response_headers) , "\r\n");
  assert(strlen(response_headers) < sizeof(response_headers));
  if (send(client_sock, response_headers, strlen(response_headers), MSG_NOSIGNAL) <= 0) {
    perror("send, send_file_header");
    return;
  }
#ifndef NDEBUG
  printf("sent:------\n%s\n-----------\n", response_headers);
  fflush(stdout);
#endif
}
#define SEND_LOCAL_FILE_BUF_SIZE 512
/*
 * send the file content to the socket
 */
void send_file_content(int client_socket, const char *filepath)
{
  char buf[SEND_LOCAL_FILE_BUF_SIZE]; //send 512 bytes each time
  int finish = 0;
  int num_bytes = 0;
  long long num_sent_bytes = 0;
  long long total_num_sent_bytes = 0;
  FILE *fp = fopen(filepath, "rb");
  if (fp == NULL)
  {
    fprintf(stderr, "fopen failed\n");
    return;
  }
  do {
    // the case we transmit the data downloaded from GreenBag
    /*if (gb)
      while (total_num_sent_bytes + SEND_LOCAL_FILE_BUF_SIZE > gb->last_downloaded_byte)
        usleep(100000); // wait for some data*/
    num_bytes = fread(buf, 1, sizeof(buf), fp);
#ifndef NDEBUG
    //printf("read %d bytes from %s\n", num_bytes, filepath);
#endif
    if (feof(fp))
      finish = 1;
    if (ferror(fp))
      error_die("ferror, send_file_content()\n");
    // fprintf(stderr, "begin sending  -  ");
    if((num_sent_bytes = send(client_socket, buf, num_bytes, MSG_NOSIGNAL)) == -1)
    {
      perror("send_file, connection gone");
      break;
    }
    total_num_sent_bytes += num_sent_bytes;
    //printf("%.2f\tSent %lld bytes to client\n", gettime(), num_sent_bytes);
    // fprintf(stderr, "end sending\n");
  }  while (!finish);
  fclose(fp);
}

/*
 * skip all headers
 */
void skip_headers(int client_sock)
{
  int numchars = 0;
  char buf[2048];
  do {
    numchars = recv(client_sock, buf, sizeof(buf), 0);
    if (numchars == -1)
      perror_die("read skip_header()");
#ifndef NDEBUG
    printf("skipped:------\n%.*s\n-----------\n", numchars, buf);
#endif
    if (strstr(buf,"\r\n\r\n") != NULL) {
      break;
    }
  } while (numchars >=0);
}

/*
 * inform the method not implemented
 */
void unimplemented(int client_sock, char *http_version)
{
  char response[1024];
  sprintf(response, 
"%s 501 Method Not Implemented\r\n\
%s\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML><HEAD><TITLE>Method Not Implemented\r\n\
</TITLE></HEAD>\r\n\
<BODY>HTTP request method not supported.<br>\r\n\
Currently only support GET method\r\n\
</BODY></HTML>\r\n", http_version, SERVER_STRING);
  assert(strlen(response) <= sizeof(response) - 1);
  if (send(client_sock, response, strlen(response), 0) <= 0) {
    perror("send, unimplemented");
    return;
  }
#ifndef NDEBUG
  printf("sent:------\n%s\n-----------\n", response);
  fflush(stdout);
#endif
}

/*
 * response that we do not find the requested file
 */
void not_found(int client_sock, const char *http_version)
{
  char response[1024];
  sprintf(response,
"%s 404 NOT FOUND\r\n\
%s\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML><TITLE>Not Found</TITLE>\r\n\
<BODY>The server cannot find the requested file</BODY></HTML>\r\n", http_version, SERVER_STRING);
  assert(strlen(response) < sizeof(response));
  if (send(client_sock, response, strlen(response), 0) <= 0) {
    perror("send, not_found");
    return;
  }
}


/*
 * download an url over multi connections
 */
void *gb_thread_main(void *arg)
{
  char *url = (char *)arg;
  return gb_process(url, GB_MAX_PERFORMANCE, NULL, ahttpd_argc, ahttpd_argv);
}

/*
 * check if the url points to our server
 * assume: the url begins with http://
 */
int is_url_to_our_server(const char *url)
{
  assert(strncmp(url, HTTP_SLASH, strlen(HTTP_SLASH)) == 0); 
  int address_begin_idx = strlen(HTTP_SLASH);
  char *our_servers[] = {"duke3.kaist.ac.kr", "cps.kaist.ac.kr"};
  int server_i; //server_index
  for (server_i = 0; server_i < sizeof(our_servers) / sizeof(*our_servers); ++server_i) {
#ifndef NDEBUG
//    fprintf(stderr, "%s : %d\n", our_servers[server_i], strlen(our_servers[server_i]));
#endif
    if (strncmp(url + address_begin_idx, our_servers[server_i], strlen(our_servers[server_i])) == 0)
      return 1;
  }
  return 0;
}

/*
 * check if the client request an remote URL (begin by BEGIN_REMOTE_URL)
 */
int check_url_remote(const char *url)
{
  // return 1 if equal 
  return (strncmp(url, BEGIN_REMOTE_URL, strlen(BEGIN_REMOTE_URL)) == 0); 
}

/*
 * extract remote url in the form of http://localhost:8188/?remoteurl
 * or http://127.0.0.1:8188/?remoteURL
 * return remoteurl
 */
char *extract_remote_url(const char *url)
{
  return (strchr(url, BEGIN_REMOTE_URL_MARK) + 1);
}
static struct option ahttpd_options[] =
{
  /* name     has_arg flag  val */
  { "log-filename-prefix",    1,  NULL, 'x' },
  { "run-once-mode", 0, NULL, 'r'},
  { NULL,           0,  NULL, 0  }
};
void parse_ahttpd_args(int argc, char **argv)
{
  int i;
  for (i = 0; i < argc; ++i)
  {
    if (strcmp(argv[i], "-r") == 0) 
    {
      run_once_mode = 1;
      printf("run once mode\n");
    }
#if 0
    if (strcmp(argv[i], "-r") == 0) 
    {
      printf("run once mode is not supported in multi-threaded gproxy\n");
    }
#endif
    if (strcmp(argv[i], "-x") == 0)
    {
      if (sscanf(argv[i + 1], "%s", log_filename_prefix) == 0)
         perror("Cannot parse the log filename prefix");
      else
        printf("log filename prefix is: %s\n", log_filename_prefix); 
    }
  }
}
