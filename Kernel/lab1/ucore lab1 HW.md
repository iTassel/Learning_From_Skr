### [1.2]
主引导扇区的特征

    1.0x200字节大小的空间
    2.末尾两字节为0x55AA
### [3] 
    A20地址位由8042键盘控制芯片确定，8042有2个有两个I/O端口：0x60和0x64
    因为如果A20为0,超过1MB外的地址发生回卷,会从0循环计数,而设置成开启的时候,可以访问1MB以上的地址,而不产生回卷以及异常
    
    首先禁止键盘操作,等待数据缓冲区中无数据后,操作8042芯片的输出端口（64h）的bit 1,操作8042打开或者关闭A20 Gate
        1.等待8042 Input buffer为空;
        2.发送Write 8042 Output Port （P2）命令到8042 Input buffer;
        3.等待8042 Input buffer为空;
        4.将8042 Output Port（P2）得到字节的第2位置1，然后写入8042 Input buffer;


    1. gdb> b *0x7C00
    .....
    	    cli
    	    cld
    	    xorw %ax, %ax
    	    movw %ax, %ds
    	    movw %ax, %es
    	    movw %ax, %ss # 清楚标志寄存器值,段寄存器清0
    2. 		lgdt gdtdesc #载入GDT表
    3. 
            movl %cr0, %eax
            orl $CR0_PE_ON, %eax
            movl %eax, %cr0 #进入保护模式
    4. 	    movw $PROT_MODE_DSEG, %ax
    	    movw %ax, %ds
    	    movw %ax, %es
    	    movw %ax, %fs
    	    movw %ax, %gs
    	    movw %ax, %ss
    	    movl $0x0, %ebp
    	    movl $start, %esp #初始化堆栈,设置段寄存器
    5. 进入bootmain函数
### [4]

    ```c
    static void readsect(void *dst, uint32_t secno)
    {
        // wait for disk to be ready
        waitdisk();								//等待磁盘准备
    
        outb(0x1F2, 1);                         // count = 1 设置扇区数量为1
        outb(0x1F3, secno & 0xFF);				//0x1F3 ~ 0X1F6设置LBA参数片段
        outb(0x1F4, (secno >> 8) & 0xFF);
        outb(0x1F5, (secno >> 16) & 0xFF);
        outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
        outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors and cmd 0x30 is write,发送读指令到0x1F7端口
        // wait for disk to be ready
        waitdisk();								//等待磁盘准备
    
        // read a sector
        insl(0x1F0, dst, SECTSIZE / 4);			// 读入数据
    }
    ```
    1.等待磁盘准备好
    2.发出读取扇区的命令
    3.等待磁盘准备好
    4.把磁盘扇区数据读到指定内存
    static void
    readseg(uintptr_t va, uint32_t count, uint32_t offset) {
        uintptr_t end_va = va + count;
    
        va -= offset % SECTSIZE; //对齐512字节的扇区大小
    
        uint32_t secno = (offset / SECTSIZE) + 1; //0扇区为引导 故 从1扇区开始
    
        for (; va < end_va; va += SECTSIZE, secno ++) {
            readsect((void *)va, secno);
        }
    }
    ```c
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);			//将ELF文件头利用readseg函数读取到指定地址(0x10000)
    
    if (ELFHDR->e_magic != ELF_MAGIC) {						//检测目标地址是否为ELF文件头
        goto bad;
    }
    
    struct proghdr *ph, *eph;
    
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph ++) {										//根据程序头的虚拟地址 大小 从而将程序代码段读入到指定地址
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }
    
    //根据ELF Header中e_entry 确定程序入口点,即调用内核
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();
    ```
    1.载入8个扇区空间的数据,判断是否为ELF文件头
    2.根据ELF Program Header进而载入程序段数据

### [5]
    void print_stackframe(void) {
         /* LAB1 YOUR CODE : STEP 1 */
         /* (1) call read_ebp() to get the value of ebp. the type is (uint32_t);
          * (2) call read_eip() to get the value of eip. the type is (uint32_t);
          * (3) from 0 .. STACKFRAME_DEPTH
          *    (3.1) printf value of ebp, eip
          *    (3.2) (uint32_t)calling arguments [0..4] = the contents in address (uint32_t)ebp +2 [0..4]
          *    (3.3) cprintf("\n");
          *    (3.4) call print_debuginfo(eip-1) to print the C calling function name and line number, etc.
          *    (3.5) popup a calling stackframe
          *           NOTICE: the calling funciton's return addr eip  = ss:[ebp+4]
          *                   the calling funciton's ebp = ss:[ebp]
          */
         unsigned int ebp,eip,r,l;
         ebp = read_ebp();
         eip = read_eip();
         for (r = 0; r < STACKFRAME_DEPTH;r++)
         {
             cprintf("EBP:0x%8x EIP:0x%8x Args: ",ebp,eip);
             unsigned int *ptr = (unsigned int*)ebp + 2;
             for( l = 0;l < 4;l++)
             {
                 cprintf("0x%8x ", ptr[l]);
             }
             cprintf("\n");
             print_debuginfo(eip - 1);
             eip = ((unsigned int*)ebp)[1];
             ebp = ((unsigned int *)eip)[0];
         }
    }

### [6、7]
    ```c
    /* Gate descriptors for interrupts and traps */
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
    每个表项大小为8字节
    其中2-3字节是段选择子，0-1字节和6-7字节拼成偏移量,通过段选择子去GDT中找到对应的基地址，然后基地址加上偏移量就是中断处理程序的地址



```c
void idt_init(void) {
     /* LAB1 YOUR CODE : STEP 2 */
     /* (1) Where are the entry addrs of each Interrupt Service Routine (ISR)?
      *     All ISR's entry addrs are stored in __vectors. where is uintptr_t __vectors[] ?
      *     __vectors[] is in kern/trap/vector.S which is produced by tools/vector.c
      *     (try "make" command in lab1, then you will find vector.S in kern/trap DIR)
      *     You can use  "extern uintptr_t __vectors[];" to define this extern variable which will be used later.
      * (2) Now you should setup the entries of ISR in Interrupt Description Table (IDT).
      *     Can you see idt[256] in this file? Yes, it's IDT! you can use SETGATE macro to setup each item of IDT
      * (3) After setup the contents of IDT, you will let CPU know where is the IDT by using 'lidt' instruction.
      *     You don't know the meaning of this instruction? just google it! and check the libs/x86.h to know more.
      *     Notice: the argument of lidt is idt_pd. try to find it!
      */
     extern uintptr_t __vectors[]; //定义256个long型外部变量,之后会用到
     uint32_t end_s = sizeof(idt) / sizeof(struct gatedesc);

    for(int i =0;i < end_s;i++){
        SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL); //向idt[i]中写入段中偏移量低16位为__vectors[i]的低16位,类型为IDT表,段选择子为kernel的程序段,描述特权为Kernel特权0
    }
    SETGATE(idt[T_SWITCH_TOK], 0, GD_KTEXT, __vectors[T_SWITCH_TOK], DPL_USER); //设置idt[121]处的中断描述符 段选择子为kernel的程序段(memlayout.h),段中偏移量低16位为__vectors[T_SWITCH_TOK]的低16位,以及描述特权为用户态权限3
    lidt(&idt_pd); //通过lidt寄存器 初始化 中断描述符表 lidt位于x86.h的头文件中
}
```

```c
// /kern/trap/trap.c
struct trapframe switchk2u, *switchu2k; //首先定义两个结构体指针
static void
trap_dispatch(struct trapframe *tf) {
    char c;

    switch (tf->tf_trapno) {
    case IRQ_OFFSET + IRQ_TIMER:
        /* LAB1 YOUR CODE : STEP 3 */
        /* handle the timer interrupt */
        /* (1) After a timer interrupt, you should record this event using a global variable (increase it), such as ticks in kern/driver/clock.c
         * (2) Every TICK_NUM cycle, you can print some info using a funciton, such as print_ticks().
         * (3) Too Simple? Yes, I think so!
         */
        ticks++;
        if (ticks % TICK_NUM == 0)
        {
            print_ticks();
        }
        //每秒调用print_ticks()函数打印一次100ticks
        break;
    case IRQ_OFFSET + IRQ_COM1:
        c = cons_getc();
        cprintf("serial [%03d] %c\n", c, c);
        break;
    case IRQ_OFFSET + IRQ_KBD:
        c = cons_getc();
        cprintf("kbd [%03d] %c\n", c, c);
        break;
    //LAB1 CHALLENGE 1 : YOUR CODE you should modify below codes.
    case T_SWITCH_TOU:
        if (tf->tf_cs != USER_CS) //判断是否为用户态CS,CPL？
        {
            switchk2u = *tf; //设置tf指针指向的trapframe结构体中的参数为用户的段寄存器
            switchk2u.tf_cs = USER_CS;
            switchk2u.tf_ds = switchk2u.tf_es = switchk2u.tf_ss = USER_DS;
            switchk2u.tf_esp = (uint32_t)tf + sizeof(struct trapframe) - 8; //设置esp的值

            switchk2u.tf_eflags |= FL_IOPL_MASK; //添加对IO的修改权限,可以在用户态模式下读写IO

            *((uint32_t *)tf - 1) = (uint32_t)&switchk2u; //切换栈空间,栈空间为设置的临时栈,iret返回时,会从switchk2u处开始恢复数据
        }
        break;
    case T_SWITCH_TOK:
        if (tf->tf_cs != KERNEL_CS) //判断是否为内核态CS
        {
            tf->tf_cs = KERNEL_CS; //替换段寄存器为内核态的段寄存器
            tf->tf_ds = tf->tf_es = KERNEL_DS;
            tf->tf_eflags &= ~FL_IOPL_MASK; //取消FL_IOPL_MASK掩码,禁止了用户态对IO的读写
            switchu2k = (struct trapframe *)(tf->tf_esp - (sizeof(struct trapframe) - 8));
            //拷贝sizeof(struct trapframe) - 8大小的数据到switchu2k临时栈
            memmove(switchu2k, tf, sizeof(struct trapframe) - 8);
            *((uint32_t *)tf - 1) = (uint32_t)switchu2k;
        }
        break;
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
        /* do nothing */
        break;
    default:
        // in kernel, it must be a mistake
        if ((tf->tf_cs & 3) == 0) {
            print_trapframe(tf);
            panic("unexpected trap in kernel.\n");
        }
    }
}
```

```c
//kern_init()中的对 lab1_switch_test(); 的注释取消
static void
lab1_switch_to_user(void) {
    //LAB1 CHALLENGE 1 : TODO
    //从中断返回时，会多pop两位，并用这两位的值更新ss、sp， 所以要先把栈压两位。
	asm volatile (
	    "sub $0x8, %%esp \n"
	    "int %0 \n"
	    "movl %%ebp, %%esp"
	    : 
	    : "i"(T_SWITCH_TOU)
	);
}

static void
lab1_switch_to_kernel(void) {
    //LAB1 CHALLENGE 1 :  TODO
	asm volatile (
	    "int %0 \n"
	    "movl %%ebp, %%esp \n"
	    : 
	    : "i"(T_SWITCH_TOK)
	);
}
//对init.c中 lab1_switch_to_user与lab1_switch_to_kernel 进行修改
```

