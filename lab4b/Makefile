#NAME: Kevin Zhang                                                                                                                       
#EMAIL: kevin.zhang.13499@gmail.com                                                                                                      
#ID: 104939334     
default:
	gcc -lmraa -lm  -Wall -Wextra -g -o lab4b lab4b.c
check: default
	chmod +x smoketest.sh
	./smoketest.sh
clean:
	rm -f lab4b *.o *.tar.gz *.txt
dist:
	tar -czvf lab4b-104939334.tar.gz Makefile README lab4b.c smoketest.sh

