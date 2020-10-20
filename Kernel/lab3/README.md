

## 相关结构

### vma_struct

```
struct vma_struct {
    // 指向了一个mm_struct结构体,此结构体描述了同一页目录表内所有虚拟内存空间的属性
    struct mm_struct *vm_mm;
    uintptr_t vm_start; 	// 一个连续地址的虚拟内存空间的起始位置
    uintptr_t vm_end; 		// 一个连续地址的虚拟内存空间的结束位置
    uint32_t vm_flags; 		// 此虚拟内存空间的rwx属性
    // 由小到大将用vma_struct表示的虚拟内存空间双向链表连接
    list_entry_t list_link;
};
```

```
#define VM_READ 0x00000001 	//只读
#define VM_WRITE 0x00000002	//可读写
#define VM_EXEC 0x00000004	//可执行
```

### mm_struct

```
struct mm_struct {
    // 将同一页目录表的所有vma_struct结构体表示的虚拟内存空间的链表头
    list_entry_t mmap_list;
    // 考虑局部性,此空间之后将会用到,避免虚拟内存空间的短时间内再次查询,提高效率
    struct vma_struct *mmap_cache;
    pde_t *pgdir; 	// mmap_list 链表中vma对应的页表项
    int map_count; 	// mmap_list 链表中vma结构体的数量
    void *sm_priv; 	//  链接记录页访问情况的链表头*
};
```

<img src="https://objectkuan.gitbooks.io/ucore-docs/lab3_figs/image001.png" alt="Head"  />

### 相关函数

```
vma_struct:
	vma_create:			生成一个vma_struct,用于管理传入的虚拟内存空间范围与属性
    insert_vma_struct:	按照[vma->vm_start,vma->vm_end]由小到大插入到mm指针指向的mm_struct中mmap_list链表
    find_vma:			根据传入的参数地址address和mm指针,从mm结构体的mmap_list链表中查找包含此address的vma_struct
mm_struct:
	mm_create:			生成一个mm_struct结构体,由kmalloc分配一处空间
	mm_destroy:		   	删除一个mm_struct结构体,由kfree释放
```



## Page Fault异常处理

##### 原因

```
1. 页表项为0,不存在映射关系
2. 相应物理页帧不在内存中(页表项非0,但页存在,swap分区 或者 磁盘中)
3. 无访问权限(Present为1,但低权限访问高权限内存空间 OR 程序试图写属性只读的页)
```

```
页访问异常错误码有32位.位0为１表示对应物理页不存在;位１为１表示写异常(比如写了只读页;位２为１表示访问权限异常(比如用户态程序访问内核空间的数据))
```

##### errorCode

##### ![](https://s1.ax1x.com/2020/07/12/U8VrO1.png)



```
EXT External event (bit 0):			值为 1 时表示硬件外部中断.
IDT Descriptor location (bit 1):	置为 1 时表示errorcode 的索引部分引用的是 IDT 的一个门描述符,置为 0 时表示引用 GDT 或者当前 LDT 的描述符.
TI (bit 2):							只在 IDT 位为 0 时有用.此时 TI 表示errorcode 的索引部分是 LDT,为 1 是是 GDT.
```

##### Page Fault errorCode

![](https://s1.ax1x.com/2020/07/12/U8VRYD.png)

```
1. 产生异常的线性地址保存在CR2寄存器,进入内核态,压入EFLAGS,CS,EIP,errorCode,以保存当前中断的程序现场
2. CPU将中断号0xE对应的中断服务例程的地址加载到CS和EIP寄存器中,开始执行中断服务例程
3. ucore开始处理异常中断,保存硬件没有保存的寄存器
4. 在vectors.S中vector14处将中断号压入内核栈，然后在trapentry.S中的标号__alltraps处把DS、ES和其他通用寄存器都压栈
5. 在trap.c的trap函数开始了中断服务例程的处理流程
   调用顺序为:trap--> trap_dispatch-->pgfault_handler-->do_pgfault[主要函数]
```

##### CR2寄存器

![CR2](https://img2018.cnblogs.com/blog/1029347/201908/1029347-20190827145842598-1082413151.png)

##### do_pgfault中对异常的处理

```
根据CR2中的物理地址以及errorCode的错误类型查找此地址是否在某vma结构描述范围内,以及内存权限是否正确
   如果全符合,则是合法访问,但没有建立映射,则分配一个空闲的内存页,修改页表完成虚地址到物理地址的映射,刷新TLB,然后调用iret中断
返回到产生页访问异常的指令处重新执行此指令.
   如果该虚地址不在某vma结构体描述的范围内,则判定为非法访问.
```



## 页替换算法

##### 先进先出(First In First Out, FIFO)页替换算法

```
算法总是淘汰最先进入内存的页,在内存中驻留时间最久的页予以淘汰,将调入内存的页以先后形成一个队列的结构
	缺点:适合程序以线性顺序访问地址空间效果才好,否则效率不高
		在增加放置页的页帧的情况下，反而使页访问异常次数增多
```

##### 时钟(Clock)页替换算法

```
时钟页替换算法把各个页面组织成环形链表的形式,类似于一个钟的表面,指针指向最先进入的内存页
当该页被访问时,CPU中的MMU硬件将把访问位置"1"
当操作系统需要淘汰页时,对当前指针指向的页所对应的页表项进行查询
	如果访问位为"0",则淘汰此页,如果此页被写过,则换到磁盘中
	如果访问位为"0",则将页表项的此位 置"0",继续访问下一页
与FIFO算法类似,但是跳过了访问位为1的页
```

##### 改进的时钟(Enhanced Clock)页替换算法

```
在时钟算法的基础上,加上了一位修改位,用于表示页是否被写过
	(0,0) 优先淘汰,未修改也未访问
	(0,1) 其次淘汰,未访问但被修改
	(1,0) 再次选择,被修改但未访问
	(1,1) 最后选择,最近使用且修改
此算法与时钟算法相比,进一步减少了磁盘的I/O操作次数,但为了寻找到可淘汰的页,需多次进行扫描,降低了算法的执行效率
```

#### 虚拟内存中页与硬盘中扇区的对应关系

```
swap_entry_t
-------------------------
| offset | reserved | 0 |
-------------------------
24 bits   7 bits    1 bit
```

```
bit 0  :	如果页被置换到硬盘中,最低位present为0,即 PTE_P标记为0,表示虚实地址映射关系不存在
bit 1-7:	暂时保留,之后用于各种扩展
bit 8-24:	表示此页在硬盘上的起始扇区的位置[扇区index索引]
为了区别页表项中0 与 swap分区的映射,将swap分区中的索引为0的扇区空出来,则页表项中高24位不为0,则两者即可区分
```

```
当程序运行中访问内存产生page fault异常时,如何判定这个引起异常的虚拟地址内存访问是越界、写只读页的"非法地址"访问还是由于数据被临时换出到磁盘上或还没有分配内存的“合法地址”访问?
	将异常的结果储存在Page Fault errorCode中,其中的bit 0 1 2可表示异常原因
何时进行请求调页/页换入换出处理?
	当访问的异常地址属于某个vma描述的范围内,但保存在swap分区中,则是执行页换入的时机,调用swap_in函数完成页面换入
	换出分积极换出策略和消极换出策略
		积极换出策略:	指操作系统周期性地(或在系统不忙的时候)主动把某些认为“不常用”的页换出到硬盘上,确保系统中总有一定数量的空闲页存在
		消极换出策略：只是当试图得到空闲页时,发现当前没有空闲的物理页可供分配,才会将不常用的页换出到硬盘
如何在现有ucore的基础上实现页替换算法?
	......
```

