# WebServer101
A simple web server html

## Building and Running

```bash
    git clone https://github.com/Jmallone/simple-web-server-html-.git
    cd simple-web-server-html-/
    make
```
Running in background
```bash
    nohup serverd &
```
Acess browser in:
http://localhost:8080

## Config File
A small config file in .serverConfig:
```bash
    #Root Dir
    ./www/

    #Root File
    index.html

    #port
    8080

    #end
    #Dont change order or position
```

### REFERENCES
1. [C Programming in Linux Tutorial #098 - A Simple Web Server Program](https://www.youtube.com/watch?v=Q1bHO4VbUck&feature=youtu.be&t=789)
2. [Sistemas de Operação – Sockets](https://www.dcc.fc.up.pt/~ines/aulas/0910/SO/sockets.pdf)
3. [Basic-Web-Server - akshay14ce1078](https://github.com/akshay14ce1078/Basic-Web-Server/blob/master/myserver.c)
4. [Simple web server en C utilizando sockets](https://kriversia.com/2017/08/simple-web-server-en-c)
5. [C Library - <string.h>](https://www.tutorialspoint.com/c_standard_library/string_h.htm)
6. [How do I concatenate two strings in C?](https://stackoverflow.com/questions/8465006/how-do-i-concatenate-two-strings-in-c/8465083)
7. [A Web Server in C](https://dzone.com/articles/web-server-c)
8. [Pico HTTP Server in C](https://gist.github.com/laobubu/d6d0e9beb934b60b2e552c2d03e1409e)
