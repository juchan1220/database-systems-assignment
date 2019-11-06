.SUFFIXES: .c .o

CC=gcc

SRCDIR=src/
INC=include/
LIBS=lib/

# SRCS:=$(wildcard src/*.c)
# OBJS:=$(SRCS:.c=.o)

# main source file
TARGET_SRC:=$(SRCDIR)main.c
TARGET_OBJ:=$(SRCDIR)main.o

# Include more files if you write another source file.
SRCS_FOR_LIB:=$(SRCDIR)bpt.c $(SRCDIR)bptservice.c $(SRCDIR)filemanager.c $(SRCDIR)buffermanager.c
OBJS_FOR_LIB:=$(SRCS_FOR_LIB:.c=.o)

SRC_BPT_SERVICE:=$(SRCDIR)bptservice.c
SRC_BPT:=$(SRCDIR)bpt.c
SRC_FILE_MANAGER:=$(SRCDIR)filemanager.c
SRC_BUFFER_MANAGER:=$(SRCDIR)buffermanager.c

OBJ_BPT_SERVICE:=$(SRCDIR)bptservice.o
OBJ_BPT:=$(SRCDIR)bpt.o
OBJ_FILE_MANAGER:=$(SRCDIR)filemanager.o
OBJ_BUFFER_MANAGER:=$(SRCDIR)buffermanager.o


CFLAGS+= -g -fPIC -I $(INC)

TARGET=main

all: $(TARGET)

$(TARGET_OBJ):
	$(CC) $(CFLAGS) -o $(TARGET_OBJ) -c $(TARGET_SRC)

$(TARGET): $(TARGET_OBJ)
	$(CC) $(CFLAGS) -o $(OBJ_BPT_SERVICE) -c $(SRC_BPT_SERVICE)
	$(CC) $(CFLAGS) -o $(OBJ_BPT) -c $(SRC_BPT)
	$(CC) $(CFLAGS) -o $(OBJ_FILE_MANAGER) -c $(SRC_FILE_MANAGER)
	$(CC) $(CFLAGS) -o $(OBJ_BUFFER_MANAGER) -c $(SRC_BUFFER_MANAGER)

	make static_library
	$(CC) $(CFLAGS) -o $@ $^ -L $(LIBS) -lbpt

clean:
	rm $(TARGET) $(TARGET_OBJ) $(OBJS_FOR_LIB) $(LIBS)*

library:
	gcc -shared -Wl,-soname,libbpt.so -o $(LIBS)libbpt.so $(OBJS_FOR_LIB)

static_library:
	ar cr $(LIBS)libbpt.a $(OBJS_FOR_LIB)
