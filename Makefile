VERSION=0.1

CFLAGS=-g -Wall
LDFLAGS=-g
LIBS=-lpopt

OBJS=chkconfig.o leveldb.o

all: chkconfig

chkconfig: $(OBJS)
	$(CC) $(LDFLAGS) -o chkconfig $(OBJS) $(LIBS)

chkconfig.o: chkconfig.c leveldb.h
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c chkconfig.c

leveldb.o: leveldb.c leveldb.h

clean:
	rm -f chkconfig $(OBJS)
