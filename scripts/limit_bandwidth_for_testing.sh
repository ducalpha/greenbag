#!/bin/bash
# Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
# Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)


# reset
if [[ "$1" == "clear" ]]
then
  echo "Clear bandwidth limits" 
  sudo tcdel --device eth0
  sudo tcdel --device eth1
elif [[ "$1" == "set" ]]
then
  echo "Set bandwidth limits"
  # set
  sudo tcset --device eth0 --rate 26M --delay 50 --direction incoming && sudo tcset --device eth1 --rate 40M --delay 50 --direction incoming
else
  echo "Usage: $0 <clear|set>"
fi

# check
tcshow --device eth0 && tcshow --device eth1 
