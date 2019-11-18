.SUFFIXES: .c .o

CC=gcc

SRCDIR=src/
INC=include/
LIBS=lib/

# SRCS:=$(wildcard src/*.c)
# OBJS:=$(SRCS:.c=.o)

# main source file
TARGET_SRC:=$(SRCDIR)main.cpp
TARGET_OBJ:=$(SRCDIR)main.o

# Include more files if you write another source file.
SRCS_FOR_LIB:=$(SRCDIR)bpt.cpp $(SRCDIR)bptservice.cpp $(SRCDIR)filemanager.cpp $(SRCDIR)buffermanager.cpp $(SRCDIR)lockmanager.cpp
OBJS_FOR_LIB:=$(SRCS_FOR_LIB:.cpp=.o)

SRC_BPT_SERVICE:=$(SRCDIR)bptservice.cpp
SRC_BPT:=$(SRCDIR)bpt.cpp
SRC_FILE_MANAGER:=$(SRCDIR)filemanager.cpp
SRC_BUFFER_MANAGER:=$(SRCDIR)buffermanager.cpp
SRC_LOCK_MANAGER:=$(SRCDIR)lockmanager.cpp

OBJ_BPT_SERVICE:=$(SRCDIR)bptservice.o
OBJ_BPT:=$(SRCDIR)bpt.o
OBJ_FILE_MANAGER:=$(SRCDIR)filemanager.o
OBJ_BUFFER_MANAGER:=$(SRCDIR)buffermanager.o
OBJ_LOCK_MANAGER:=$(SRCDIR)lockmanager.o

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
	$(CC) $(CFLAGS) -o $(OBJ_LOCK_MANAGER) -c $(SRC_LOCK_MANAGER)

	make static_library
	$(CC) $(CFLAGS) -o $@ $^ -L $(LIBS) -lbpt

clean:
	rm $(TARGET) $(TARGET_OBJ) $(OBJS_FOR_LIB) $(LIBS)*

library:
	gcc -shared -Wl,-soname,libbpt.so -o $(LIBS)libbpt.so $(OBJS_FOR_LIB)

static_library:
	ar cr $(LIBS)libbpt.a $(OBJS_FOR_LIB)
