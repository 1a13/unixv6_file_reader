# Makefile for CS 111 project to read Unix V6 file system

CC = /usr/bin/clang-10

PROGS = diskimageaccess

LIB_SRCS  = diskimg.c inode.c unixfilesystem.c directory.c pathname.c chksumfile.c file.c

DEPS = -MMD -MF $(@:.o=.d)
WARNINGS = -fstack-protector -Wall -W -Wcast-qual -Wwrite-strings -Wextra \
	-Wno-unused-parameter -Wno-unused-function

CFLAGS = -g -fno-limit-debug-info $(WARNINGS) $(DEPS) -std=gnu99
LDFLAGS =

LIB_OBJS = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(LIB_SRCS)))
LIB_DEPS = $(patsubst %.o,%.d,$(LIB_OBJS))
LIB = v6fslib.a

PROG_SRCS = diskimageaccess.c
PROG_OBJS = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(PROG_SRCS)))
PROG_DEPS = $(patsubst %.o,%.d,$(PROG_OBJS))

TMP_PATH := /usr/bin:$(PATH)
export PATH = $(TMP_PATH)

LIBS += -lssl -lcrypto -lm

# This auto-commits changes on a successful make and if the tool_run environment variable is not set (it is set
# by tools like sanitycheck, which run make on the student's behalf, and which already commmit).
# The very long piped git command is a hack to get the "tools git username" used
# when we make the project, and use that same git username when committing here.
all:: $(PROGS)
	@retval=$$?;\
	if [ -z "$$tool_run" ]; then\
		if [ $$retval -eq 0 ]; then\
			git diff --quiet --exit-code;\
			retval=$$?;\
			if [ $$retval -eq 1 ]; then\
				git log tools/create | grep 'Author' -m 1 | cut -d : -f 2 | cut -c 2- | xargs -I{} git commit -a -m "successful make." --author={} --quiet;\
				git push --quiet;\
				fi;\
		fi;\
	fi


diskimageaccess: diskimageaccess.o $(LIB)
	$(CC) $(LDFLAGS) diskimageaccess.o $(LIB) $(LIBS) -o $@

$(LIB): $(LIB_OBJS)
	rm -f $@
	ar r $@ $^
	ranlib $@

clean::
	rm -f $(PROGS) $(PROG_OBJS) $(PROG_DEPS)
	rm -f $(LIB) $(LIB_DEPS) $(LIB_OBJS)

.PHONY: all clean

-include $(LIB_DEPS) $(PROG_DEPS)

