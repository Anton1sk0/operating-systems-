OBJS	= FileReader.o main.o
SOURCE	= FileReader.c main.c
HEADER	= FileReader.h limits.h semmun.h
OUT	= test.out
CC	 = gcc
FLAGS	 = -g -c -Wall
LFLAGS	 = 

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

FileReader.o: FileReader.c
	$(CC) $(FLAGS) FileReader.c 

main.o: main.c
	$(CC) $(FLAGS) main.c 


clean:
	rm -f $(OBJS) $(OUT)
