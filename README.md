voidmq
======

A message queue foundation. This is a working basic client/server model.

Initial Testing
---------------

You can compile the server with:
```gcc server.c -o server```

You can compile the threaded server with:
```gcc server_pthreads.c -o server_pthreads -lpthread```

You can compile the client with:
```gcc client.c -o client```


Invoke the traditional (forked) server with:
```./server```

Invoke the client in another terminal with:
```./client "DATA TO SEND"```

You should see the output from the server stating it recieved the payload
The server then sends the payload back to the client
At which point states it also received it!

Shut down the running server with `CTRL+C`, and then

Invoke the threaded server with:
```./server_pthreads```

Now, run the same client command several times
Up to 11 to be specific, that is when the thread pool runs out of available threads
