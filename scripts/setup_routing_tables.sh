#!/bin/bash
# Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
# Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

# Assume the machine has 2 interfaces with same default gateway

if [[ "$#" -ne 5 ]]
then
  echo "Usage: $0 <interface0> <ip0> <interface1> <ip1> <default_gate_way>"
  echo "Example: $0 eth0 10.0.0.2 eth1 10.0.0.3 10.0.0.1"
  echo "Run ifconfig for the parameters"
  exit -1
fi

if0=$1
ip0=$2
if1=$3
ip1=$4
default_gateway=$5

sudo ip rule add from $ip0 lookup 2 prio 20
sudo ip rule add from $ip1 lookup 3 prio 30

sudo ip ro add ta 2 $ip0 dev $if0 scope link src $ip0

sudo ip ro add ta 2 default via $default_gateway dev $if0
sudo ip ro add ta 3 $ip1 dev $if1 scope link src $ip1
sudo ip ro add ta 3 default via $default_gateway dev $if1

# After running the above, the routing tables should be like following
# $ ip ru
# 0:      from all lookup local 
# 20:     from $ip0 lookup 2 
# 30:     from $ip1 lookup 3 
# 32766:  from all lookup main 
# 32767:  from all lookup default 
# 
# $ ip ro
# default via $default_gateway dev $if1 onlink 
# $some_subnet/24 dev $if1  proto kernel  scope link  src $ip1 
# $some_subnet/24 dev $if0  proto kernel  scope link  src $ip0 
# 
# $ ip ro sh ta 2
# default via $default_gateway dev $if0 
# $ip0 dev $if0  scope link  src $ip0 
# 
# $ ip ro sh ta 3
# default via $default_gateway dev $if1 
# $ip1 dev $if1  scope link  src $ip1 


#########################################################
# When 2 links are on different subnets
# #Set addr and link
# ip link set dev eth1 up
# ip link set dev eth2 up
# ip addr add 10.0.2.2/24 dev eth1
# ip addr add 10.0.3.2/24 dev eth2
# 
# #Set rules and rtables
# ip rule add from 10.0.2.2 lookup 2 prio 20
# ip rule add from 10.0.3.2 lookup 3 prio 30
# 
# ip ro add ta 2 10.0.2.0/24 dev eth1 scope link src 10.0.2.2
# ip ro add ta 2 default via 10.0.2.1 dev eth1
# 
# ip ro add ta 3 10.0.3.0/24 dev eth2 scope link src 10.0.3.2
# ip ro add ta 3 default via 10.0.3.1 dev eth2
# 
# ip ro add 10.0.1.0/24 via 10.0.2.1 dev eth1
# =-----=---------
# ip ro del default via $some_default_gateway dev eth0
# ip ro add default via 10.0.3.1 dev eth2
# ip ro del default via 10.0.3.1 dev eth2
# ip ro add default via $some_default_gateway dev eth0
# 
# ip ru
# 0:      from all lookup local
# 32764:  from 10.0.3.2 lookup 3
# 32765:  from 10.0.2.2 lookup 2
# 32766:  from all lookup main
# 32767:  from all lookup default
# 
# ip ro
# $some_subnet/24 dev eth0  proto kernel  scope link  src $some_ip
# 10.0.1.0/24 via 10.0.2.1 dev eth1
# 10.0.2.0/24 dev eth1  proto kernel  scope link  src 10.0.2.2
# 10.0.3.0/24 dev eth2  proto kernel  scope link  src 10.0.3.2
# default via 10.0.3.1 dev eth2
# 
# ip ro sh ta 2
# 10.0.2.0/24 dev eth1  scope link  src 10.0.2.2
# default via 10.0.2.1 dev eth1
# 
# ip ro sh ta 3
# 10.0.3.0/24 dev eth2  scope link  src 10.0.3.2
# default via 10.0.3.1 dev eth2
