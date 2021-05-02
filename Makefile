#
# File:
#    Makefile
#
# Description:
#
# $Date$
# $Rev$
#

ARCH			= `uname`-`uname -m`
CROSS_COMPILE		=
CC			= $(CROSS_COMPILE)g++
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -O2 -fno-exceptions -std=c++0x -fexceptions -fPIC -I/usr/include\
			-I. \
			-I./xilinx_hls_lib_2017_4 \
			-I$(CODA)/$(ARCH)/include \
			`root-config --cflags`
LINKLIBS		= -lrt \
	  		-L$(CODA)/$(ARCH)/lib \
	  		-levioxx -lexpat -levio \
			`root-config --libs` \
			`root-config --glibs`
PROGS			= cmzero_evio
HEADERS			= $(wildcard *.h)
SRC			= cmzero_evio.cc mpd_data_processor.cpp mpd_data_processor_wrapper.cpp
OBJS			= $(SRC:.C=.o)

all: $(PROGS) $(HEADERS)

clean distclean:
	@rm -f bin/$(PROGS) *~ *.o outlinkDef.{C,h}

%.o:	%.C Makefile
	@echo "Building $@"
	$(CC) $(CFLAGS) \
	-c $<

$(PROGS): $(OBJS) $(SRC) $(HEADERS) Makefile
	
	@echo "Building $@"
	$(CC) $(CFLAGS) -o bin/$@ \
	$(OBJS) \
	$(LINKLIBS)


.PHONY: all clean distclean
