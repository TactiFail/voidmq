SRCDIR=src
BINDIR=bin

all: dirs server client
threaded: dirs server-threaded client

# Make sure binary dir exists
dirs: $(BINDIR)
$(BINDIR):
	mkdir -p $(BINDIR)

server:
	gcc $(SRCDIR)/server.c -o $(BINDIR)/server

server-threaded:
	gcc $(SRCDIR)/server_pthreads.c -o $(BINDIR)/server_threaded -lpthread

client:
	gcc $(SRCDIR)/client.c -o $(BINDIR)/client

.PHONY: clean dirs
clean:
	rm $(BINDIR)/*
