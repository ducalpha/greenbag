
# GreenBag

## Introduction

Modern mobile devices are equipped with multiple network interfaces, including 3G/LTE and WiFi. Bandwidth aggregation over LTE and WiFi links offers an attractive opportunity for supporting bandwidth-intensive services, such as high-quality video streaming, on mobile devices.

[GreenBag project](http://cps.kaist.ac.kr/greenbag) introduces an energy-efficient bandwidth aggregation middleware that supports real-time data-streaming services over asymmetric wireless links, requiring no modification to the existing Internet infrastructure. The [paper](http://cps.kaist.ac.kr/papers/13RTSS_GreenBag_PID2926259.pdf) was published on the 34th IEEE Real-Time Systems Symposium (RTSS '13).

![Overview](docs/greenbag_overview.png)

## Demo

### Dual-link vs single-link download speed

[![Dual link vs single link](http://img.youtube.com/vi/m63600JUN-M/0.jpg)](https://www.youtube.com/watch?v=m63600JUN-M)

## No more video interruption time

[![Playback Time Comparison](http://img.youtube.com/vi/FMFNJi2X-Yc/0.jpg)](https://www.youtube.com/watch?v=FMFNJi2X-Yc)

## Energy-efficient video streaming

[![Energy Consumption Comparison](http://img.youtube.com/vi/MqXcb8vExsg/0.jpg)](https://www.youtube.com/watch?v=MqXcb8vExsg)

## Before building
GreenBag was designed to download via LTE and WiFi but current version does not automatically detect those network interfaces in the system. Therefore, you need to add the interface names to the corresponding lookup lists in [lookup_if_names.c](https://github.com/ducalpha/greenbag/blob/master/src/lookup_if_names.c). On a desktop with multiple network interfaces, you need to specify one into the LTE list and the other into the WiFi list.

## How to build
This source code can be built for Android and Linux desktop platforms. The following build commands for desktops were tested with gcc 5.4.0 on Ubuntu 16.04. You can build on Android by using gcc in Android NDK.

* Standalone mode: In this mode, GreenBag works like a typical downloader, such as wget.  
`make standalone`

* Proxy mode: In this mode, GreenBag acts as a proxy to between the server and the video player. GreenBag was orignally designed to work on this mode, but it requires some additional setup steps.  
`make`

## Server configuration
I tested with an Apache2 server which allows an unlimited maximum number of requests during a persistent connection. You need to add `MaxKeepAliveRequests 0` on the server's configuration file (e.g., `/etc/apache2/apache2.conf`)

## Setting routing tables for multiple network interfaces on Linux
Although Android will automatically set up routing tables correctly when there are multiple network interfaces in the system, desktop Linux distributions typically do not. You can set up the routing tables on Linux using [setup_routing_tables.sh](scripts/setup_routing_tables.sh).

## Running GreenBag
Run GreenBag with fixed segment sizes (`-F 1`), each segment is 2048 KB large (`-S 2048`)  
`./gb http://143.248.140.50:8088/files/Gangnam.mp4 -F 1 -S 2048` 

More options are in [gbsession.c](https://github.com/ducalpha/greenbag/blob/master/src/gbsession.c).

The download time should be faster than when using wget to download over a single link:  
`wget --bind-address=$ip0 http://server.com/files/1GB`  
`wget --bind-address=$ip1 http://server.com/files/1GB`

You can also set bandwidth using [tcconfig](https://github.com/thombashi/tcconfig). An example script is include in [limit_bandwidth_for_testing.sh](scripts/limit_bandwidth_for_testing.sh).

## Limitations of this version
* Does not automatically determine the optimal segment sizes. Future versions should support flexible segment sizes and be adaptive to the changing network conditions such as bandwidth and RTT.
* Does not automatically detect network interfaces in the system.
* Does not automatically determine whether a network interface is LTE or WiFi.
* Uses disk to store in-progress file parts. Future versions should try to store them in memory.
* Requires MaxKeepAliveRequests on the server side.
* Does not determine bitrates of videos automatically.
* Does not setup routing tables on client automatically.

## How to enable multiple network interfaces on Android
In order to enable LTE and WiFi simultaneously on Samsung Galaxy S2 phone where the source code of the Android framework was not available, we reversed engineered and modified ConnectivityService in the Android framework. You can find details in [How to Enable Multiple Network Interfaces on Android](https://docs.google.com/document/d/1zpRF1jbZ6egCjiRn0DGuIvaAHzBEEisHnEoX3U4oupE/edit?usp=sharing).

