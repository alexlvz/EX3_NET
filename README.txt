alexlv
Alex Levenzon 308636141
EX3
server.c - Implementation of a HTTP server
threadpool.c - implementation of threadpool that will be used by the server
README.txt - A readme text file

This program is a HTTP server. It can handle HTTP requests of type GET with HTTP method 1.1 or 1.0 , and send back to the client a corresponding responde. 
The server uses the threadpool in order to handle as maximum as 200 requests simultaniosly.

The file server.c is the main file of the program. There are 14 functions, all comments are inside of the server.c file.

In order to use the program, you have to compile the threadpool.c and server.c with threadpool.h header(use makefile) and run in the 
following order:

./server <port> <pool-size> <max-number-of-request>

Now that the server is running , you can browse from you browser in the following way:

write localhost:<port-number> 
This will bring you to the root directory of the server(showed in table) or if index.html is found - it will open it.
any other pass can be filled in after <port-number> and "/".

The server can handle with files with spaces, and traverse through the folders tree.

Beware that you have to have execution permitions for folders and read permitions for files in order to access them.

This was a really cool excercise! 

Thank you! :)