default:
		gcc -lz -Wall -Wextra -o lab1b-client lab1b-client.c
		gcc -lz -Wall -Wextra -o lab1b-server lab1b-server.c
clean:
		rm -f *.o lab1b-client lab1b-server *.tar.gz
dist: default
		tar -czf lab1b-104939334.tar.gz lab1b-client.c lab1b-server.c Makefile README
