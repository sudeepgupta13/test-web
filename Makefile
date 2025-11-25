serverd: server.c
	gcc -o serverd server.c -pthread

run: serverd
	./serverd

clean:
	rm -f serverd
