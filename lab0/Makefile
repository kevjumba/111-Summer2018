# Kevin Zhang
# kevin.zhang.13499@gmail.com
# 104939334

default:
		gcc -g -Wall -Wextra -o lab0 lab0.c
check: allTests

clean:
		rm -f *.o lab0 *.tar.gz
dist: default
		tar -czf lab0-104939334.tar.gz lab0.c Makefile test.sh backtrace.png breakpoint.png README


allTests:
		@chmod +x test.sh; \
		./test.sh
