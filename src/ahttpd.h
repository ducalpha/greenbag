// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#ifndef _AHTTPD_H_
#define _AHTTPD_H_
#define IS_SPACE(x) isspace((int)(x))
#define EXT_SEPARATOR '.'
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#define BEGIN_REMOTE_URL_MARK '?'
#define BEGIN_REMOTE_URL "/?http://"
#define GPROXY_ADDR "127.0.0.1"
#define GPROXY_PORT 8188
extern long long int nsent_bytes_to_client;
int check_url_remote(const char *url);
char *extract_remote_url(const char *url);

#endif

