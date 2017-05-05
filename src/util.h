// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

// For socket communication
#ifndef __COMM_H__
#define __COMM_H__
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include "gb.h"
#define member_size(type, member) sizeof(((type *)0)->member)
#define FRAME_DECODED "numVideoFramesDecoded"
#define MAX_LOG_FILENAME_PREFIX_LEN 32
extern char log_filename_prefix[MAX_LOG_FILENAME_PREFIX_LEN];

/* TCP control */
int tcp_connect( char *hostname, int port, char *local_if );
/* other utilities */
double gettime();
int get_if_ip( char *iface, char *ip );
/* Simple wrappers */
int Socket(int family, int type, int protocol);
int Bind(int fd, const struct sockaddr *sa, socklen_t salen);
int Listen(int fd, int backlog);
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int Close(int fd);
int Shutdown(int sockfd, int how);
int Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
off_t Lseek(int fd, off_t offset, int whence);
int Fseek(FILE *stream, long offset, int whence); 
ssize_t Write(int fd, const void *buf, size_t count);
ssize_t Read(int fildes, void *buf, size_t nbyte);
FILE *Fopen(const char *path, const char *mode);
ssize_t Send(int sockfd, const void *buf, size_t len, int flags);
int Getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int Setsockopt(int sockfd, int level, int optname,
                      const void *optval, socklen_t optlen);
int Fstat(int fd, struct stat *buf);
int Pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg);
int Pthread_join(pthread_t thread, void **retval);
int Pthread_kill(pthread_t thread, int sig);
/* error information */
void error_die(const char *format, ...);
void perror_die(const char *msg);
#define error_num_die(error_num, msg) \
  do { errno = error_num; perror(msg); exit(EXIT_FAILURE); } while(0)
// FIXME may work only if the filepath is the filename
int file_exists(const char *local_path, struct stat *file_status);
char *get_unique_filepath(char *new_filepath, char *filepath);
/* string utilities */
char *prepend(char *dst, const char *prefix);
int starts_with(const char *str, const char *prefix);
int ends_with(const char *str, const char *suffix);
long int get_bitrate(const char *url);
/* network utilities */
int make_connection(const char *ip_addr, const char *port);
int make_connection_over_local_if(const char *ip_addr, const char *port, const char *local_if);
int get_num_vframe_decoded(long long int *num_vframe_decoded);
char *get_ip_dev(char *dev_name);
int decompose_url(char *url, char **protocol, char **host, char **port, char **abs_path);
int get_online_if(char if_array[][16]);
char *rewrite_url_gproxy(char *file_url);
int lookup_if_name(char *found_if_name, int lookup_if_index);
char *get_log_filename_prefix_args(char *found_prefix, int argc, char **argv);
void reset_getopt(void);
/* gprofiler utilities */
void *activate_gprofiler(void *file_url_arg);
void deactivate_gprofiler(void);
#endif

