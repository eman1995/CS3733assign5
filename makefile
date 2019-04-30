CC      =gcc
CFLAGS  =-g -pthread
LDFLAGS =-lpthread
OBJFILES=assign5.o
TARGET  =assign5

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LDFLAGS)

clean:
	rm -f $(OBJFILES) $(TARGET) *~
#gcc -g -Wall -pthread assign5.c -lpthread -o assign5
