// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

// For socket communication
#include "util.h"
#include "ahttpd.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h> // isspace()
#include <signal.h> // pthread_kill
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include "config.h"
#include "lookup_if_names.h"
#define HTTP_PORT "80"
#define DEBUG
char log_filename_prefix[MAX_LOG_FILENAME_PREFIX_LEN] = "";

/* Create a TCP connection
 * return the socket that connect to the hostname:port over local_if
 */
#define MAX_RETRY 50
int tcp_connect( char *hostname, int port, char *local_if )
{
  struct hostent *host = NULL;
  struct sockaddr_in address;
  struct sockaddr_in local;
  int sock;
  int i;
  int retry = 1;
  struct timeval connect_timeout;
  fd_set fdset;
  int sock_opts;
  // lookup ip address of the hostname
	/* Why this loop? Because the call might return an empty record.*/
  for (i = 0; i < 5; ++i) {
    if ((host = gethostbyname(hostname)) == NULL)
      return -1;
    if (*host->h_name)
      break;
  }
  if (!host || !host->h_name || !*host->h_name)
    return -1;

  // create a socket that binds to a local if
  while (retry <= MAX_RETRY)
  {
#ifdef DEBUG
    socklen_t local_len = sizeof(local);
    fprintf(stderr, "tcp_connect( %s, %i ) = ", hostname, port);
#endif
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
      return -1;

    if (local_if && *local_if)
    {
      local.sin_family = AF_INET;
      local.sin_port = 0;
      local.sin_addr.s_addr = inet_addr(local_if);
      if (bind(sock, (struct sockaddr *)&local, sizeof(struct sockaddr_in)) == -1)
      {
        close(sock);
        return -1;
      }
    }
    // create address
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr = *((struct in_addr *)host->h_addr);
    if ((sock_opts = fcntl(sock, F_SETFL, O_NONBLOCK)) == -1) perror("fcntl failed\n");

    // non-blocking connect
    connect(sock, (struct sockaddr *)&address, sizeof(address));
    // set connect_timeout
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    connect_timeout.tv_sec = 3;
    connect_timeout.tv_usec = 0;

    fprintf(stderr, "Connecting ...\n");
    if (select(sock + 1, NULL, &fdset, NULL, &connect_timeout) == 1)
    {
      int so_error = 0;
      socklen_t so_error_len = sizeof(so_error);
      getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &so_error_len);
      if (so_error == 0)
      {
#ifdef DEBUG
        getsockname(sock, (struct sockaddr *)&local, &local_len);
        fprintf(stderr, "%i\n", ntohs( local.sin_port ) );
#endif
        sock_opts = sock_opts & (~O_NONBLOCK);
        if (fcntl(sock, F_SETFL, sock_opts) == -1) perror("fcntl failed\n");
        return sock;
      }
      else
      {
        perror("so_error");
        fprintf(stderr, "retry: %d\n", retry++);
        close(sock);
      }
    }
    else
    {
      perror("select");
      fprintf(stderr, "retry: %d\n", retry++);
      close(sock);
    }
  }
  close(sock);
  return -1;
}
/* time() with more precision						*/
double gettime()
{
  struct timespec current_time;
  clock_gettime(CLOCK_MONOTONIC, &current_time);
	return( (double) current_time.tv_sec + (double) current_time.tv_nsec / 1000000000 );
}
int get_if_ip( char *iface, char *ip )
{
	struct ifreq ifr;
	int fd = socket( PF_INET, SOCK_DGRAM, IPPROTO_IP );
	
	memset( &ifr, 0, sizeof( struct ifreq ) );
	
	strcpy( ifr.ifr_name, iface );
	ifr.ifr_addr.sa_family = AF_INET;
	if( ioctl( fd, SIOCGIFADDR, &ifr ) == 0 )
	{
		struct sockaddr_in *x = (struct sockaddr_in *) &ifr.ifr_addr;
		strcpy( ip, inet_ntoa( x->sin_addr ) );
		return( 1 );
	}
	else
	{
		return( 0 );
	}
}

int Socket(int family, int type, int protocol)
{
  int n;
  if((n = socket(family, type, protocol)) == -1) {
    perror("Socket creation error");
    exit(EXIT_FAILURE);
  }
  return n;
}
int Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
  int n;
  if((n = bind(fd, sa, salen) == -1)) {
    struct sockaddr_in *addr = (struct sockaddr_in *)sa;
    //inet_ntop(sa->sa_family, &((struct sockaddr_in *)sa->sin_addr), addr, sizeof(sa));
    perror("Socket binding error");
    fprintf(stderr, "Address %s:%d\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    exit(EXIT_FAILURE);
  }
  return n;
}
int Listen(int fd, int backlog)
{
  int n;
  if((n = listen(fd, backlog)) == -1) {
    perror("Socket listening error");
    exit(EXIT_FAILURE);
  }
  return n;
}
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  int n;
  if((n = accept(sockfd, addr, addrlen)) == -1) {
    perror("Socket accept error");
    exit(EXIT_FAILURE);
  }
  return n;
}
int Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
  int n;
  if((n = connect(sockfd, addr, addrlen)) < 0) { // supposed to be -1
    perror("Socket connect error");
    exit(EXIT_FAILURE);
  }
  return n;
}
int Close(int fd)
{
  int status = close(fd);
  if (status == -1)
    perror_die("Socket close failed");
  return status;
}
int Shutdown(int sockfd, int how)
{
  int status = shutdown(sockfd, how);
  if (status == -1)
    perror_die("Socket shutdown failed");
  return status;
}


off_t Lseek(int fd, off_t offset, int whence)
{
  off_t n;
  if ((n = lseek(fd, offset, whence)) < 0) { // supposed to be -1
    perror("lseek");
    exit(EXIT_FAILURE);
  }
  return n;
}
int Fseek(FILE *stream, long offset, int whence)
{
  int ret = fseek(stream, offset, whence);
  if (ret == -1) {
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  return ret;
}
ssize_t Write(int fd, const void *buf, size_t count)
{
  ssize_t write_bytes;
  write_bytes = write(fd, buf, count);
  if (write_bytes != count) {
    perror("write failed");
    exit(EXIT_FAILURE); // FIXME should I stop here?
  }
  return write_bytes;
}
ssize_t Read(int fildes, void *buf, size_t nbyte)
{
  ssize_t read_bytes = read(fildes, buf, nbyte);
  if (read_bytes == -1) {
    perror("read");
    exit(EXIT_FAILURE);
  }
  return read_bytes;
}
FILE *Fopen(const char *path, const char *mode)
{
  FILE *file = fopen(path, mode);
  if (file == NULL)
    perror_die("fopen failed");
  return file;
}
ssize_t Send(int sockfd, const void *buf, size_t len, int flags)
{
  ssize_t sent_bytes = send(sockfd, buf, len, flags);
  if (sent_bytes == -1) {
    perror("send, socket gone");
    //close(sockfd);
    exit(5);
  }
  return sent_bytes;
}
int Getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  int ret = getsockname(sockfd, addr, addrlen);
  if (ret == -1) {
    perror("getsockname");
    exit(EXIT_FAILURE);
  }
  return ret;
}
int Setsockopt(int sockfd, int level, int optname,
                      const void *optval, socklen_t optlen)
{
  int ret = setsockopt(sockfd, level, optname, optval, optlen);
  if (ret == -1) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
  return ret;
}
int Fstat(int fd, struct stat *buf)
{
  int ret = fstat(fd, buf);
  if (ret == -1) {
    perror("fstat");
    exit(EXIT_FAILURE);
  }
  return ret;
}

int Pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg)
{
  int err_num = pthread_create(thread, attr, start_routine, arg);
  if (err_num != 0)
    error_num_die(err_num, "pthread_create");
  return err_num;
}
int Pthread_join(pthread_t thread, void **retval)
{
  int err_num = pthread_join(thread, retval);
  if (err_num != 0)
    error_num_die(err_num, "pthread_join");
  return err_num;
}
int Pthread_kill(pthread_t thread, int sig)
{
  int err_num = pthread_kill(thread, sig);
  if (err_num != 0) {
    if (err_num == ESRCH) {
      fprintf(stderr, "err_num == ESRCH: %d\n", err_num == ESRCH);
      fprintf(stderr, "no thread with the ID %lu\n", thread);
    }
    error_num_die(err_num, "pthread_kill failed");
  }
  return err_num;
}

/*
 * print out an error message with perror() then exit
 */
void perror_die(const char *msg)
{
  perror(msg);
#ifndef PTHREAD
  exit(3);
#else
  pthread_exit(NULL);
#endif
}
/*
 * print out the error message and exit
 */
void error_die(const char *format, ...)
{
  va_list varg_list;
  va_start(varg_list, format);
  vfprintf(stderr, format, varg_list);
  va_end(varg_list);
#ifndef PTHREAD
  exit(EXIT_FAILURE);
#else
  pthread_exit(NULL);
#endif
}
/*
 * check file existence
 */
int file_exists(const char *local_path, struct stat *file_status)
{
  return (stat(local_path, file_status) != -1);
}
/*
 * create a new and unique file name, return the pointer to the filename
 */
char *get_unique_filepath(char *new_filepath, char *filepath)
{
  int file_index = 1;
  FILE *new_log = NULL;
  //char *new_filepath = malloc(512 * sizeof(char));// MAX chars in a file anem is set at 512
  char ext[16] = ""; // file extension
  char *ext_pos = NULL;

  // copy the file extension
//  printf("file path: %s\n", filepath);
  strcpy(new_filepath, filepath);
  if ((ext_pos = strrchr(filepath, EXT_SEPARATOR)) != NULL)
  {
    strcpy(ext, ext_pos + 1);
    *strrchr(new_filepath, EXT_SEPARATOR) = '\0';
  }
//  printf("file extension: %s\n", ext);
  char date_id[32];
  time_t now;
  time(&now);
  if(strftime(date_id, sizeof(date_id), "%j.%H%M", localtime(&now)))
    sprintf(new_filepath + strlen(new_filepath), ".%s.%s", date_id, ext);
  if ((new_log = fopen(new_filepath, "r"))) {
    fclose(new_log);
    char *new_filepath_ext = strrchr(new_filepath, EXT_SEPARATOR);
    sprintf(new_filepath_ext, ".%i.%s", file_index, ext);
    while ((new_log = fopen(new_filepath, "r"))) { // file exists
      fclose(new_log);
      sprintf(new_filepath_ext, ".%i.%s", ++file_index, ext);
    }
  } 
  //return prepend(new_filepath, log_filename_prefix);
  return new_filepath;
}
char *prepend(char *dst, const char *prefix)
{
  size_t prefix_len = strlen(prefix);
  memmove(dst + prefix_len, dst, strlen(dst) + 1); // include NULL of dst
  memcpy(dst, prefix, prefix_len);
  return dst;
}
int starts_with(const char *str, const char *prefix)
{
  size_t lenstr = strlen(str);
  size_t lenprefix = strlen(prefix);
  return lenstr < lenprefix?0:strncmp(prefix, str, lenprefix) == 0;
}
int ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return (strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0);
}
#define DICT_FILEPATH "bitrate_dict"
#define COMMENT_LINE_MARKER "#"
/* return -1 if cannot find the bitrate */
long int get_bitrate(const char *url)
{
  char f_url[512];
  int bitrate;
  char line[2048];
  FILE *bitrate_dict = fopen(DICT_FILEPATH, "r");
  //printf("Searching for URL: %s\n", url);
  if (bitrate_dict == NULL) {
    printf("Cannot open bitrate dictionary: %s\n", strerror(errno));
    return -1;
  }
  while (fgets(line, sizeof(line), bitrate_dict) != NULL) {
    //printf("read line: %s", line);
    if (starts_with(line, COMMENT_LINE_MARKER))
      continue;
    sscanf(line, "%s\t%d", f_url, &bitrate);
    assert(strlen(f_url) < sizeof(f_url));
    //printf("url: %s\tbitrate: %d\n", f_url, bitrate);
    if (strcmp(f_url, url) == 0)
      return bitrate;
  }
  perror("reading bitrate dictionary end");
  return -1;
}

int make_connection(const char *ip_addr, const char *port)
{
  int server_sock;
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(port));
  if(inet_pton(AF_INET, ip_addr, &server_addr.sin_addr) <= 0)
    perror_die("inet_pton failed");
  server_sock = Socket(AF_INET, SOCK_STREAM, 0);
  Connect(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr));
  return server_sock;
}

/* local_if: address x.x.x.x of a local interface */
int make_connection_over_local_if(const char *ip_addr, const char *port, const char *local_if)
{
  int server_sock;
  struct sockaddr_in server_addr, local;

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(port));
  if(inet_pton(AF_INET, ip_addr, &server_addr.sin_addr) <= 0)
    perror_die("inet_pton failed");
  server_sock = Socket(AF_INET, SOCK_STREAM, 0);

  if (local_if) {
    printf("\nMake connection over local if: %s\n", local_if);
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = 0;
    local.sin_addr.s_addr = inet_addr(local_if);
    if (bind(server_sock, (struct sockaddr *)&local, sizeof(struct sockaddr_in)) == -1) {
      close(server_sock);
      perror("make_connection_over_local_if failed");
      return -1;
    }
  }

  Connect(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr));
  return server_sock;
}

int get_num_vframe_decoded(long long int *num_vframe_decoded)
{
  FILE *dumpsys = NULL;
  char out_dumpsys[2048];
//#ifndef OS_ANDROID
#if 1
  int dumpsys_fd = 0;
  int hi_fd = 0;
  int num_read_fd = 0;
  struct timeval select_timeout;
#endif
  char *num_vframes_pos = NULL;
  dumpsys = popen("dumpsys media.player", "r");
  if ((dumpsys == NULL)) { // || (dumpsys == -1)) {
    perror("popen");
    return FAILURE_STATUS;
  }
//#ifndef OS_ANDROID
#if 1
  if ((dumpsys_fd = fileno(dumpsys)) == -1) {
    fprintf(stderr, "cannot get file descriptor from dumpsys pipe %s\n", strerror(errno));
    goto failure;
  }
//    fprintf(stderr, "log_fps popen and get fileno are fine, now select\n"); // sometimes stops here!
  fd_set read_set;
  FD_ZERO(&read_set);
  FD_SET(dumpsys_fd, &read_set);
  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 100000; //100ms
  hi_fd = dumpsys_fd;
  num_read_fd = select(hi_fd + 1, &read_set, NULL, NULL, &select_timeout);
  if (num_read_fd == -1) {
    fprintf(stderr, "select() failed: %s\n", strerror(errno));
    goto failure;
  } else if (num_read_fd == 0) {
    fprintf(stderr, "cannot read dumpsys within timeout\n");
    goto failure;
  }
#endif
  /* success to read dumpsys */
  if (fread(out_dumpsys, 1, sizeof(out_dumpsys), dumpsys)) {
    if ((num_vframes_pos = strstr(out_dumpsys, FRAME_DECODED))) {
      sscanf(num_vframes_pos, FRAME_DECODED "(%lld),%*s", num_vframe_decoded);
    }
  }
  //printf("Read from dumpsys: %s\n", out_dumpsys);
  if (pclose(dumpsys) == -1) fprintf(stderr, "pclose dumpsys failed: %s\n", strerror(errno));
  return SUCCESS_STATUS;
failure: 
  //if (pclose(dumpsys) == -1) fprintf(stderr, "pclose dumpsys failed: %s\n", strerror(errno));
  return FAILURE_STATUS;
}
/* return ip address of the device */
char *get_ip_dev(char *dev_name)
{
  struct ifreq ifr;
  int fd;
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  // I want to get an IPv4 addr 
  ifr.ifr_addr.sa_family = AF_INET;
  // I want IP addres attached to dev_name
  strncpy(ifr.ifr_name, dev_name, IFNAMSIZ-1);
  ioctl(fd, SIOCGIFADDR, &ifr);
  close(fd);
  return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
} 
/* 
 * allocate memory for returning protocol, host, port, and abs_path
 */
int decompose_url(char *url, char **protocol, char **host, char **port, char **abs_path)
{
  char *start_marker = url;
  char *marker;
  char *port_colon;
  /* trim leading spaces */
  while (isspace(*start_marker)) start_marker++;
  if (starts_with(start_marker, "http://") == 0)
    error_die("The input url must start with \"http://\"\n");
  *protocol = strndup(start_marker, strlen("http"));
  start_marker = start_marker + strlen("http://");
  marker = strchr(start_marker, '/');
  *marker = '\0';
  if ((port_colon = strchr(start_marker, ':')) != NULL) {
    *port_colon = '\0';
    *host = strdup(start_marker);
    *port_colon = ':';
    start_marker = port_colon + 1;
    *port = strdup(start_marker);
  } else {
    *port = strdup(HTTP_PORT);
    *host = strdup(start_marker);
  }
  *marker = '/';
  *abs_path = strdup(marker);
  printf("url: %s\n", url);
  printf("protocol: %s\n", *protocol);
  printf("host: %s\n", *host);
  printf("port: %s\n", *port);
  printf("abs path: %s\n", *abs_path);
  return SUCCESS_STATUS;
}
/* store online interface names into the if_array */
/* return number of online interfaces */
int get_online_if(char if_array[][16])
{
  char buf[2048];
  struct ifconf ifconfig;
  struct ifreq *ifr;
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  ifconfig.ifc_len = sizeof(buf);
  ifconfig.ifc_buf = buf;
  if (ioctl(sock, SIOCGIFCONF, &ifconfig) < 0)
  {
    perror("ioctl(SIOCGIFCONF)");
    return 1;
  }
  close(sock);
  ifr = ifconfig.ifc_req;
  int num_if = ifconfig.ifc_len / sizeof (struct ifreq);
  int i;
  printf("Number of online interfaces: %d\n", num_if);
  for (i = 0; i < num_if; ++i)
  {
    strcpy(if_array[i], ifr[i].ifr_name);
  }
  return i;
}
char *rewrite_url_gproxy(char *file_url)
{
  char *new_url = NULL;
  asprintf(&new_url, "http://%s:%d/?%s", GPROXY_ADDR, GPROXY_PORT, file_url);
  return new_url;
}
/* lookup LTE or WiFi-emulated interface name from if_name list */
/* NOTE: The first match will be used, the order of names in lookup_if_names is important */
/* return SUCCESS_STATUS on success */
int lookup_if_name(char *found_if_name, int lookup_if_index)
{
  char online_ifs[64][16]; // support up to 64 interfaces, each interface can have up to 16 characters
  int num_online_ifs = get_online_if(online_ifs);
  int i, j;
  char **if_names = NULL;
  switch (lookup_if_index) {
    case INDEX_LTE:
      if_names = lte_if_names;
      break;
    case INDEX_WIFI:
      if_names = wifi_if_names;
      break;
    default:
      fprintf(stderr, "unknown lookup interface index\n");
      return FAILURE_STATUS;
  }
  if (num_online_ifs == 0) {
    fprintf(stderr, "There is no online interfaces\n");
    pthread_exit(NULL);
  }
  //for (i = 0; i < sizeof(lookup_if_names) / sizeof(*lookup_if_names); ++i) {
  for (i = 0; if_names[i] != NULL; ++i) {
    for (j = 0; j < num_online_ifs; ++j) {
      //printf("lookup if name: %s\tonline if: %s\n", if_names[i], online_ifs[j]);
      if (strcmp(if_names[i], online_ifs[j]) == 0) {
        strcpy(found_if_name, if_names[i]);
        return SUCCESS_STATUS;
      }
    }
  }
  return FAILURE_STATUS;
}
static struct option log_filename_prefix_option[] =
{
  /* name     has_arg flag  val */
  { "log_filename_prefix",    1,  NULL, 'x' },
  { NULL,           0,  NULL, 0  }
};
char *get_log_filename_prefix_args(char *found_prefix, int argc, char **argv)
{
  int option;
  opterr = 0;
  while (1)
  {
    if ((option = getopt_long(argc, argv, "n:o:S::vi:1:2:Q:F:f:E:W:A:B:l:x:", log_filename_prefix_option, NULL)) == -1)
      break;
    switch(option)
    {
      case 'x':
        //fprintf(stderr, "option x with argument %s\n", optarg);
        //sscanf(optarg, "%s", found_prefix);
        //fprintf(stderr, "option x with argument %s\n", found_prefix);
        if (sscanf(optarg, "%s", found_prefix) == 0)
          return NULL;
        break;
    }
  }
  return found_prefix;
}

#define MAX_FILEPATH_LEN 512
/* note: the output file path must be long enough to store the generated filepath */
char *gen_unique_filepath(char *output_filepath, char *base_filepath, time_t time_tag)
{
  char new_filepath[MAX_FILEPATH_LEN + 24];// MAX chars in a filepath is long :)
  char ext[8] = ""; // file extension
  FILE *new_log = NULL;
  char date_id[32];
  int file_index = 1;
  /* find the extesion dot in the base file path */
  strcpy(new_filepath, base_filepath);
  strcpy(ext, strrchr(base_filepath, EXT_SEPARATOR) + 1);
  *strrchr(new_filepath, EXT_SEPARATOR) = '\0';
  /* add time tag */
  if(strftime(date_id, sizeof(date_id), "%j.%H%M", localtime(&time_tag)))
    sprintf(new_filepath + strlen(new_filepath), ".%s.%s", date_id, ext);
  if ((new_log = fopen(new_filepath, "r"))) {
    fclose(new_log);
    char *new_filepath_ext = strrchr(new_filepath, EXT_SEPARATOR);
    sprintf(new_filepath_ext, ".%i.%s", file_index, ext);
    while ((new_log = fopen(new_filepath, "r"))) { // file exists
      fclose(new_log);
      sprintf(new_filepath_ext, ".%i.%s", ++file_index, ext);
    }
  }
  strcpy(output_filepath, new_filepath);
  return output_filepath;
}
void reset_getopt(void)
{
  optind = 1; // do not parse argv[0], the program name
}
void *activate_gprofiler(void *file_url_arg)
{
  char *file_url = (char *)file_url_arg;
  int gprofiler_socket = 0;
  char gprofiler_port[8];
  sprintf(gprofiler_port, "%d", GPROFILER_PORT);
  gprofiler_socket = make_connection(GPROFILER_IP_ADDR, gprofiler_port);
  Send(gprofiler_socket, file_url, strlen(file_url), 0);
  close(gprofiler_socket);
  return NULL;
}
void deactivate_gprofiler(void)
{
  int gprofiler_stopper_socket = 0;
  char gprofiler_stopper_port[8];
  sprintf(gprofiler_stopper_port, "%d", GPROFILER_STOPPER_PORT);
  gprofiler_stopper_socket = make_connection(GPROFILER_IP_ADDR, gprofiler_stopper_port);
  Send(gprofiler_stopper_socket, GPROFILER_STOP_MSG, strlen(GPROFILER_STOP_MSG), 0);
  Shutdown(gprofiler_stopper_socket, SHUT_RDWR);
  Close(gprofiler_stopper_socket);
  printf("Deactivated gprofiler\n");
}
/*
int is_interface_online(char *interface)
{
  struct ifreq ifr;
  int sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP);
  memset(&ifr, 0, sizeof(ifr));
  strcpy(ifr.ifr_name, interface);
  if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
    perror("SIOCGIFFLAGS: failed to get status of interface");
  close(sock);
  return !!(ifr.ifr_flags & IFF_UP);
}*/
