// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

/* GreenBag global settings */
#ifdef OS_ANDROID
#define VIDEO_FPS 30 // assume video fps is 30
//#define VIDEO_FPS 23.98 // Bourne.mp4
#endif
#define GPROFILER_IP_ADDR "127.0.0.1"
#define GPROFILER_PORT 8288
#define GPROFILER_STOPPER_PORT 8289
#define INDEX_LTE 0
#define INDEX_WIFI 1
#ifdef OS_ANDROID
#define NUM_VFRAMES_DECODED_FILEPATH "/data/gproxy/num_decoded_vframes"
#else
#define NUM_VFRAMES_DECODED_FILEPATH "/tmp/num_decoded_vframes"
#endif
#define START_MSG "msg:start"
#define GPROFILER_STOP_MSG "Msg: Stop\r\n"
#define USE_COND
#define ENABLE_UPDATE_RTT 0
#define ENABLE_MEASURE_RTT 0
#define ENABLE_GB_LOG 0
#define ENABLE_LOG_GOODPUT 0
#define ENABLE_LOG_GOODPUT_TO_CLIENT 0
#define ENABLE_LOG_RECOVERY 0
#define LOG_RECOVERY_FILEPATH "log_recovery.txt"
#define GPROXY_PID_FILEPATH "gproxy_pid"
#define RTT_LOG_FILEPATH "log_rtt_getsockopt.txt"
#define GOODPUT_LOG_FILEPATH "log_goodput.txt"
#define FPS_LOG_FILEPATH "log_fps.txt"
#define REPORT_FILEPATH "session_report.txt"
#define GOODPUT_TO_CLIENT_LOG_FILEPATH "log_goodput_to_client.txt"
#define BASIC_TIMEOUT 500000  //micro sec
#define LOW_FPS_THRESHOLD 0 // fps
#define VERBOSITY 1 // 0: no verbose, 1: less verbose, 2 more verbose
#define NUM_CONNECTIONS 2
#define ETCDIR "/usr/local/etc"
#define _REENTRANT
#define _THREAD_SAFE
#define ARCH "Linux"
#define GB_VERSION_STRING	"2.4"
#define DEFAULT_USER_AGENT	"GreenBag " GB_VERSION_STRING " (" ARCH ")"
/* Compiled-in settings							*/
#define MAX_STRING		1024
#define MAX_ADD_HEADERS	10
#define MAX_REDIR		5
// no support for internationalization
#define _( x )  x 

#define SUCCESS_STATUS 0
#define FAILURE_STATUS -1
