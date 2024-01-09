CC = gcc
CFLAGS = -g -Wall

mysh: mysh.c
	$(CC) $(CFLAGS) mysh.c -o mysh

clean:
	rm -f mysh



