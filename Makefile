###############################################
#
# Make file for Distributed file server
# Author: Sunil bn <sunhick@gmail.com>
#
# ############################################

CC = g++
DBUG = -g -O0
LDFLAGS = -pthread
CCFLAGS = -Wall -std=c++11 $(DBUG) -I $(IDIR)

IDIR = ./src/include
WEB_PROXY = webproxy

# Source files for dfs
WEB_PROXY_SRC = src/logger.cc src/webproxy.cc src/main.cc
WEB_PROXY_OBJS = $(WEB_PROXY_SRC:.cc=.o)

OBJS = $(WEB_PROXY_OBJS)

# SRC = $(\wildcard df*.cc)
# OBJS = $(\SRC:.cc=.o)

TARBALL = webproxy.tar.gz

ifeq ($(DEBUG), 1)
	DBUG += -DDEBUG
endif

all: $(WEB_PROXY) $(DFC) $(TEST)

$(WEB_PROXY): $(WEB_PROXY_OBJS)
	$(CC) $(WEB_PROXY_OBJS) -o $@ $(LDFLAGS)

%.o: %.cc
	$(CC) -c -o $@ $(CCFLAGS) $<

.PHONY: clean
clean:
	@rm $(OBJS) $(WEB_PROXY) src/*~ src/include/*~ $(TARBALL)

# create a source tar ball 
tar:
	tar -cvzf $(TARBALL) src/*.* src/include/*.* README Makefile
