SRCDIR=./src
BINDIR=./bin

all: server client

server:
	gcc $(SRCDIR)/server.c -o $(BINDIR)/server

client:
	gcc $(SRCDIR)/client.c -o $(BINDIR)/client

.PHONY: clean
clean:
	rm $(BINDIR)/*
