# Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
# Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

OPTIMIZED_COMPILE=0
CFLAGS=-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I.. -Wall -Wno-unused-result -Wno-unused#for asprintf
ifeq ($(PLATFORM), arm)
GPROXY_DIR_ANDROID=/data/gproxy
ANDROID_NDK=/home/ducalpha/adr/ndk
CC=${ANDROID_NDK}/toolchains/arm-linux-androideabi-4.6/prebuilt/linux-x86/bin/arm-linux-androideabi-gcc
CFLAGS+=--sysroot=${ANDROID_NDK}/platforms/android-14/arch-arm -DANDROID -DOS_ANDROID
CFLAGS+=-O1 #always optimized on Android because we still cannot debug on Android
LFLAGS=--sysroot=${ANDROID_NDK}/platforms/android-14/arch-arm -fPIC -mandroid -L/home/ducalpha/adr/ndk/platforms/android-14/arch-arm/usr/lib
else
CC=gcc
ifeq ($(OPTIMIZED_COMPILE), 1)
CFLAGS+=-O1 
else
CFLAGS+=-O0 -g
endif
LFLAGS=-lpthread -lrt# -lpcap
endif

COMMON=gb.o conf.o conn.o http.o util.o algo_general.o periodic_tasks.o gbsession.o lookup_if_names.o offset.o recovery.o log_params.o player_time.o energy.o
OBJS=$(COMMON) ahttpd.o 
OBJS_GB_STANDALONE=$(COMMON) gb_standalone.o 

OUTFILE=gproxy
GB_STANDALONE_OUTFILE=gb

all: $(OUTFILE)
standalone: $(OBJS_GB_STANDALONE)
	$(CC) $(OBJS_GB_STANDALONE) -o $(GB_STANDALONE_OUTFILE) $(LFLAGS)
$(OUTFILE): $(OBJS)
	$(CC) $(OBJS) -o $(OUTFILE) $(LFLAGS)
%.o: %.c
	$(CC) -c $*.c -o $*.o $(CFLAGS)

ifeq ($(PLATFORM), arm)
install: $(OUTFILE)
	adb push $(OUTFILE) $(GPROXY_DIR_ANDROID)
	adb push repeat_gproxy.sh $(GPROXY_DIR_ANDROID)
endif
ahttpd.o: ahttpd.h extra_info.h
gb.o: gb.h extra_info.h
clean:
	rm -f *.o $(OUTFILE)
