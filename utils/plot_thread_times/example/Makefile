CC=g++
CFLAGS= -g -Wall

all: multithreaded_test

multithreaded_test: multithreaded_test.cpp
	$(CC) $(CFLAGS) -o multithreaded_test multithreaded_test.cpp -lpthread

clean:
	$(RM) multithreaded_test

