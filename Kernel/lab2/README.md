### 链表指针

```
struct list_entry {
    struct list_entry *prev, *next;
};
```

用于双向链表指针的管理的结构

### 管理页的数据结构 

```c
struct Page {
    int ref;        // page frame's reference counter
    uint32_t flags; // array of flags that describe the status of the page frame
    unsigned int property;// the num of free block, used in first fit pm manager
    list_entry_t page_link;// free list link
};
```

    ref:        页被页表的引用记数
    flags:      bit0 为1表示被保留(reserved),不能分配和释放,bit1为1表示可分配状态,bit1为0表示已分配
    property:   记录某连续内存空闲块的大小,此变量为Head Page用
    page_link:  双向链表指针,管理页的分配,同样为Head Page用

### 管理内存的数据结构
当内存页逐渐的分配与释放,残留的小的连续内存空间块由free_area_t数据结构管理
```c
/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct {
            list_entry_t free_list;                                // the list header
            unsigned int nr_free;                                 // # of free pages in this free list
} free_area_t;
```
free_list:  管理双向链表的链表头
nr_free:    当前空间页的个数

### 
物理内存地址最大为maxpa,页大小为PGSIZE,那么需要管理的物理页数量为:

    npage = maxpa / PGSIZE

管理上述所有内存的Page结构所占空间为:

    sizeof(struct Page) * npage

将上述所有Page结构所占空间存放于 bootloader加载ucore的结束地址,以end指针作记录,页向上对齐后以pages指针作为作为存放的起始位置
    
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);

[pages,sizeof(struct Page) * npage]之间作为上述所有Page结构存放的内存地址

故 所需管理的内存起始地址为

    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * npage); //PADDR函数用于获取一个内核的虚拟地址(地址在KERNBASE之上)

pmm_manager用于物理内存页管理的结构

```
struct pmm_manager {
            const char *name; //物理内存页管理器的名字
            void (*init)(void); //初始化内存管理器
            void (*init_memmap)(struct Page *base, size_t n); //初始化管理空闲内存页的数据结构
            struct Page *(*alloc_pages)(size_t n); //分配n个物理内存页
            void (*free_pages)(struct Page *base, size_t n); //释放n个物理内存页
            size_t (*nr_free_pages)(void); //返回当前剩余的空闲页数
            void (*check)(void); //用于检测分配/释放实现是否正确的辅助函数
};
```

```
如何在建立页表的过程中维护全局段描述符表(GDT)和页表的关系，确保ucore能够在各个时间段上都能正常寻址?
	在建立页表的时候，此时还是链接时由bootloader生成的GDT.在建立页表完成后，需要更新全局GDT.这才真正开启了分页机制.
对于哪些物理内存空间需要建立页映射关系?
	从ucore结束的地址开始 即 pages处
具体的页映射关系是什么?
	页目录 -> 页表 -> 页
页目录表的起始地址设置在哪里?
	#define VPT                 0xFAC00000
	pde_t * const vpd = (pde_t *)PGADDR(PDX(VPT), PDX(VPT), 0);
	vpd变量的值就是页目录表的起始虚地址0xFAFEB000
页表的起始地址设置在哪里,需要多大空间?
	此时描述内核虚拟空间的页目录表的虚地址为0xFAFEB000,大小为4KB.页表的理论连续虚拟地址空间0xFAC00000~0xFB000000,大小为4MB.因为这个连续地址空间的大小为4MB,可有1M个PTE,即可映射4GB的地址空间.
	又因为ucore只支持896MB的物理内存空间,故定义 #define KERNTOP (KERNBASE + KMEMSIZE)=0xF8000000 最大为0xF8000000的内核虚地址
	所以最大内核虚地址KERNTOP的页目录项虚地址为  vpd+0xF8000000>>22 = 0xFAFEB000+0x3E0=0xFAFEB3E0
	最大内核虚地址KERNTOP的页表项虚地址为:		 vpt+0xF8000000>>12 = 0xFAC00000+0xF8000=0xFACF8000
	
如何设置页目录表项的内容?
	PTE_U:	位3,表示用户态的软件可以读取对应地址的物理内存页内容
	PTE_W:	位2,表示物理内存页内容可写
	PTE_P:	位1，表示物理内存页存在
	页目录表项的末尾比特位可以表示对应页表的存在与否 以及 读写权限
如何设置页表项的内容?
	页表项的末尾比特位可以表示对应页的存在与否 以及 读写权限
```



### 二级页表

二级页表用来管理线性地址与物理地址间的映射关系

```
假定当前物理内存0~16MB，每物理页（也称Page Frame）大小为4KB，则有4096个物理页，也就意味这有4个页目录项和4096个页表项需要设置。
一个页目录项(Page Directory Entry，PDE)和一个页表项(Page Table Entry，PTE)占4B。
即使是4个页目录项也需要一个完整的页目录表（占4KB）。而4096个页表项需要16KB（即4096*4B）的空间，也就是4个物理页，16KB的空间。
所以对16MB物理页建立一一映射的16MB虚拟页，需要5个物理页，即20KB的空间来形成二级页表。
```

lab2写得有点水,从lab2开始 需要自己编程就比较多了,等把ucore学完了再回来补充,实在不知道怎么往里面补了  

而且ucore的学习文档 和 给的ucore内核代码有点改变 有点没摸清楚关系,作业只能先抄一抄尽量搞懂

