// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include <stdio.h>
#include "lookup_if_names.h"
// First match will be used
/* NOTE: The first match will be used, the order of names in lookup_if_names is important */
char *lte_if_names[] = {"rmnet_sdio0", "rmnet_sdio1", "eth0", NULL};
char *wifi_if_names[] = {"wlan0", "eth1", NULL};
