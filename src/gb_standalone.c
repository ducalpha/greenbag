// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include <stdio.h>
#include <stdlib.h>
#include "gbsession.h"
int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "USAGE: %s <video-file-URL>\n", argv[0]);
    exit(1);
  }
  gb_process(argv[1], GB_MAX_PERFORMANCE, NULL, argc, argv);
  return 0;
}
