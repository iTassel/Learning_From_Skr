### 实模式
早期的8086处理器的一种简单运行模式,软件可访问的物理内存空间不能超过1MB,且无法发挥Intel 80386以上32位CPU的4GB内存管理能力,而且每一个指针都是指向实际的物理地址,操作系统和用户程序并没有区别对待.
### 保护模式
80386就是通过在实模式下初始化控制寄存器(如GDTR,LDTR,IDTR与TR等管理寄存器)以及页表,然后再通过设置CR0寄存器使其中的保护模式使能位置位,从而进入到80386的保护模式,其所有的32根地址线都可供寻址,物理寻址空间高达4GB,支持内存分页机制,支持优先级机制,不同的程序可以运行在不同的特权级上,内核态在权限0处,用户态在权限3处

### 启动 
启动: BIOS -> Boot Loader -> Kernel

    计算机加电后,CPU从物理地址0xFFFFFFF0处的跳转指令执行BIOS例行程序
                            |
                            V
    BIOS完成自检工作后,选择启动设备,然后加载主扇区到内存地址0x7C00
                            |
                            V
    跳转到地址0x7C00处并开始执行代码,由实模式转换到保护模式,最后执行Kernel
                            |
                            V
    Kernel从0x100000开始,开启分页机制,启动虚拟内存,对I/O进行实现

注:保护模式由A20 Gate控制,而A20 Gate通过8042键盘控制器控制,其中由0x60 和 0x64两个端口确定  
读60h端口,读output buffer  
写60h端口,写input buffer  
读64h端口,读Status Register  
写64h端口,操作Control Register，写入0x20h为读命令,写入0x60为写命令  

打开A20 Gate:  
等待8042 Input buffer为空;  
发送Write 8042 Output Port (P2)命令到8042 Input buffer;  
等待8042 Input buffer为空;  
将8042 Output Port(P2)得到字节的第2位置1,然后写入8042 Input buffer;  
### 地址空间

    +------------------+  <- 0xFFFFFFFF (4GB)
    |     无效空间      |
    +------------------+  <- addr:3G+256M 
    |       256MB      |
    |   IO外设地址空间  |
    +------------------+  <- 0xC0000000(3GB)
    |     无效空间      |
    +------------------+  <- 0x40000000(1GB)
    |                  |
    |    实际有效内存   |
    |                  |
    +------------------+  <- 0x00100000 (1MB)
    |     BIOS ROM     |
    +------------------+  <- 0x000F0000 (960KB)
    |  16-bit devices, |
    |  expansion ROMs  |
    +------------------+  <- 0x000C0000 (768KB)
    |   VGA Display    |
    +------------------+  <- 0x000A0000 (640KB)
    |                  |
    |    Low Memory    |
    |                  |
    +------------------+  <- 0x00000000

### 启动分段与未启用分页机制后计算物理地址
启动分段机制,未启动分页机制：逻辑地址--> (分段地址转换) -->线性地址==物理地址  

CPU把逻辑地址(由段选择子selector和段偏移offset组成)中段选择子的内容作为段描述符表的索引,在此描述符表索引对应处段描述符中保存的地址加上段偏移值,即为线性地址,因为未启动分页机制,故此线性地址则为物理地址  
### 启动分段与分页机制后计算物理地址

启动分段和分页机制：逻辑地址--> (分段地址转换) -->线性地址-->分页地址转换) -->物理地址  
因为Linux中所有段地址初始化都为0x00000000,所以线性地址=逻辑地址+0x000000  
如 mov eax,[0x80495B0]中  
0x8049420是逻辑地址,所以线性地址 = 0x80495B0,如果没有开启分页机制,则 0x80495B0 也是物理地址  
如果开启分页机制  
![线转物](https://img-blog.csdnimg.cn/20190308134721166.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3lldGFpYmluZzE5OTA=,size_16,color_FFFFFF,t_70)
线性地址最高10位 Directory 页目录表偏移量,中间10位 Table是页表偏移量,最低12位Offset是物理页内的字节偏移量。  
页目录表的大小为4KB(刚好是一个页的大小),包含1024项,每个项4字节(32位)  
表项里存储的内容就是页表的物理地址(因为物理页地址4k字节对齐,最低比特位记录了页读写权限),如果页目录表中的页表尚未分配,则物理地址填0  

页目录项内容 = (页表起始物理地址 &0x0FFF) | PTE_U | PTE_W | PTE_P  
页表项内容   = (pa & ~0x0FFF) | PTE_P | PTE_W  

    PTE_U:	位3,表示用户态的软件可以读取对应地址的物理内存页内容
    PTE_W:	位2,表示物理内存页内容可写
    PTE_P:	位1，表示物理内存页存在

![图片](https://objectkuan.gitbooks.io/ucore-docs/lab2_figs/image006.png)
0x80495B0转换成2进制  
0000 1000 00 |00 0100 1001| 0101 1011 0000|  

从CR3寄存器中取出进程的页目录地址,根据前10位,在目录中找到对应索引项,获取页表的地址,根据中间10位找到索引处的页表项,最终可从中找到页的起始地址,加上偏移的12bit值,即为我们想要计算出的物理地址  
如果表项的内容为0,则会引发一个缺页异常,进程暂停执行  

## GDT段表
分段机制涉及4个关键内容:逻辑地址、段描述符(描述段的属性)、段描述符表(包含多个段描述符的"数组")、段选择子(段寄存器,用于定位段描述符表中表项的索引)  
### 段选择子
段选择子16bit,段选择子偏移量是32bit.  
![图片](https://pdos.csail.mit.edu/6.828/2007/readings/i386/fig5-6.gif)
索引(Index[15:3]):用于在含有8192个描述符的描述符表中索引对应的描述符[处理器自动将索引值乘以8(描述符的长度),加上描述符表的基址来索引描述符表,从而选出一个合适的描述符]  
表指示位(Table Indicator,TI[2:2]):选择段描述符表的类型,0表示访问全局描述符表(GDT),1表示访问局部描述符表(LDT)  
请求特权级(Requested Privilege Level,RPL[1:0]):用于选择请求者的特权级,00最高,11最低  

补:  
CS拥有一个由CPU维护的当前特权级字段(Current Privilege Level,简称CPL)  
GDT的第12和13项段描述符是 __KERNEL_CS 和__KERNEL_DS,第14和15项段描述符是 __USER_CS 和__USER_DS  

### 段描述符
```c
/* segment descriptors */
struct segdesc {
    unsigned sd_lim_15_0 : 16;          // low bits of segment limit
    unsigned sd_base_15_0 : 16;         // low bits of segment base address
    unsigned sd_base_23_16 : 8;         // middle bits of segment base address
    unsigned sd_type : 4;               // segment type (see STS_ constants)
    unsigned sd_s : 1;                  // 0 = system, 1 = application
    unsigned sd_dpl : 2;                // descriptor Privilege Level
    unsigned sd_p : 1;                  // present
    unsigned sd_lim_19_16 : 4;          // high bits of segment limit
    unsigned sd_avl : 1;                // unused (available for software use)
    unsigned sd_rsv1 : 1;               // reserved
    unsigned sd_db : 1;                 // 0 = 16-bit segment, 1 = 32-bit segment
    unsigned sd_g : 1;                  // granularity: limit scaled by 4K when set
    unsigned sd_base_31_24 : 8;         // high bits of segment base address
};
```
![示意图](https://pdos.csail.mit.edu/6.828/2007/readings/i386/fig5-3.gif)

    BASE(32位)：段首地址的线性地址。
    G(Granularity)：为0代表此段长度以字节为单位,为1代表此段长度以4K为单位。
    LIMIT(20位)：   此最后一个地址的偏移量,也相当于长度,G=0,段大小在1~1MB,G=1,段大小为4KB~4GB。
    S：             为0表示是系统段,否则为代码段或数据段。
    Type：          描述段的类型和段是否可读/写/执行.
    DPL：           描述符特权级,表示访问这个段CPU要求的最小优先级(保存在cs寄存器的CPL特权级),用来实现保护机制.
    P：             表示此段是否被交换到磁盘,总是置为1,因为linux不会把一个段都交换到磁盘中。
    D或B：          如果段的LIMIT是32位长,则置1,如果是16位长,置0.(详见intel手册)

### 硬盘访问

    IO地址	功能
    0x1f0	读数据，当0x1f7不为忙状态时,可以读.
    0x1f2	要读写的扇区数,每次读写前,你需要表明你要读写几个扇区.最小是1个扇区
    0x1f3	如果是LBA模式,就是LBA参数的0-7位
    0x1f4	如果是LBA模式,就是LBA参数的8-15位
    0x1f5	如果是LBA模式,就是LBA参数的16-23位
    0x1f6	第0~3位：如果是LBA模式就是24-27位 第4位：为0主盘；为1从盘
    0x1f7	状态和命令寄存器.操作时先给命令,再读取,如果不是忙状态就从0x1f0端口读数据

实现如下

    1.等待磁盘准备好
    2.发出读取扇区的命令
    3.等待磁盘准备好
    4.把磁盘扇区数据读到指定内存

具体可参考boot/bootmain.c中的readsect函数实现

```c
static void waitdisk(void) {
    while ((inb(0x1F7) & 0xC0) != 0x40)
        /* do nothing */;
}

/* readsect - read a single sector at @secno into @dst */
static void readsect(void *dst, uint32_t secno) {
    // wait for disk to be ready
    waitdisk();

    outb(0x1F2, 1);                         // count = 1
    outb(0x1F3, secno & 0xFF);              // LBA[0:7]
    outb(0x1F4, (secno >> 8) & 0xFF);       // LBA[8:15]
    outb(0x1F5, (secno >> 16) & 0xFF);      // LBA[16:23]
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0); // LBA[24:27] , the LBA[28:28] is 0 for MainDisk and  1 for SecondaryDisk
    outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors and cmd 0x60 is write

    // wait for disk to be ready
    waitdisk();

    // read a sector
    insl(0x1F0, dst, SECTSIZE / 4);
}
```

## 中断与异常
中断描述符表（Interrupt Descriptor Table):
IDT和GDT类似,是一个8字节的描述符数组,IDT起始地址保存在IDT寄存器（IDTR）中,指令LIDT和SIDT用来操作IDTR
### 三种特殊的中断事件:

    中断:  产生的时刻不确定的中断,与CPU的执行无关,我们称之为异步中断(asynchronous interrupt)也称外部中断,简称中断(interrupt)
    异常:  CPU执行指令期间检测到不正常的或非法的条件,所引起的内部事件称作同步中断(synchronous interrupt)
    软中断:在程序中使用请求系统服务的系统调用而引发的事件,称作陷入中断(trap interrupt)
### 

中断向量表一个表项占用8字节,其中2-3字节是段选择子,0-1字节和6-7字节拼成偏移量  
LIDT(Load  IDT Register):   使用一个包含线性地址基址和界限的内存操作数来加载IDT,只能在特权级0执行,创建IDT时执行它初始化IDT的起始地址,将IDT的地址加载到IDTR寄存器中  
SIDT(Store IDT Register):   拷贝IDTR的基址和界限部分到一个内存地址,在任意特权级执行  

中断描述符
```c
struct gatedesc {
    unsigned gd_off_15_0 : 16;        // low 16 bits of offset in segment
    unsigned gd_ss : 16;              // segment selector
    unsigned gd_args : 5;             // # args, 0 for interrupt/trap gates
    unsigned gd_rsv1 : 3;             // reserved(should be zero I guess)
    unsigned gd_type : 4;             // type(STS_{TG,IG32,TG32})
    unsigned gd_s : 1;                // must be 0 (system)
    unsigned gd_dpl : 2;              // descriptor(meaning new) privilege level
    unsigned gd_p : 1;                // Present
    unsigned gd_off_31_16 : 16;       // high bits of offset in segment
};
```
![中断](https://i.stack.imgur.com/ItKOI.png)
### 中断处理
1. 系统接受到中断后,产生中断向量  
2. 根据中断向量在IDT中索引中断描述符,中断描述符里保存着中断服务例程的段选择子  
3. 利用中断描述符中的段选择子在GDT中索引段描述符,其中保存有中断服务例程的段基址和属性信息  
4. 将中断服务例程的基地址与中断描述符中的偏移Offset(中断向量在中断向量表中的地址)相加得到中断服务例程的地址  
5. 跳转到此中断服务例程的起始地址,并执行代码  

注:其中CPU会根据CPL和中断服务例程的段描述符的DPL信息确认是否发生了特权级的转换  
![处理](http://linuxeco.com/wp-content/uploads/2012/01/Interrupt-Procedure-Call1.png)


在保护模式下,最多会存在256个Interrupt/Exception Vectors,范围[0,31]内的32个向量被异常Exception和NMI使用,范围[32,255]内的向量被保留给用户定义的Interrupts  
