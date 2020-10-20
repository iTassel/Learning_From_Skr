## LAB1
### Exercise1、2
    ......
### Exercise3

    1. 从0x7C23开始对CR0寄存器|1,PE=1,开启32位保护模式,0x7C2D跳转到0x7C32处开始执行32位代码
    2. 引导加载程序最后一条指令为"0x7D6B: call *0x10018",内核加载第一条指令为 0x10000C: movw $0x1234,0x472
    3. 内核加载第一条指令在0x10000C
    4. 引导加载程序通过ELF Program header 获取信息

### Exercise4
    ......

### Exercise5

    在/boot/Makefrag中将0x7C00修改,然后调试断点在0x7C00,汇编指令未变,但是0x7C2D处跳转出错
    "ljmp $PROT_MODE_CSEG, $protcseg"指令因为Makefrag中加载地址改变,所以此处获取到错误的跳转地址

### Exercise6

    1. 引导加载程序执行初,0x100000中8个内存字为0
    2. 引导加载程序执行进入内核时,0x100000处存在数据
    E. 引导加载程序将内核加载到物理内存0x100000中

### Exercise7
    1. 内核程序在执行"movl %eax, %cr0"前,0x00100000有数据,0xF0100000为0
    2. 内核程序在执行"movl %eax, %cr0"后,0x00100000和0xF0100000具有相同的数据
    E. 处理器的内存管理硬件将0xF0100000映射到物理地址0x00100000
    3. 通过"set $eip=0x100028"绕过"movl %eax, %cr0",在"0x10002D: jmp *%eax"会跳转到0xF010002F,紧接着报错
        ···
        #Turn on paging.
        movl    %cr0, %eax
        orl $(CR0_PE|CR0_PG|CR0_WP), %eax
        movl    %eax, %cr0
        ···
    ···
    // Control Register flags
    #define CR0_PE      0x00000001  // Protection Enable
    #define CR0_MP      0x00000002  // Monitor coProcessor
    #define CR0_EM      0x00000004  // Emulation
    #define CR0_TS      0x00000008  // Task Switched
    #define CR0_ET      0x00000010  // Extension Type
    #define CR0_NE      0x00000020  // Numeric Errror
    #define CR0_WP      0x00010000  // Write Protect
    #define CR0_AM      0x00040000  // Alignment Mask
    #define CR0_NW      0x20000000  // Not Writethrough
    #define CR0_CD      0x40000000  // Cache Disable
    #define CR0_PG      0x80000000  // Paging
    ···

### Exercise8

    1. console.c中函数cputchar()接口供printf.c中putch()处调用
    2. 利用memmove 将crt_buf+80 拷贝2000-80个字节 到 将crt_buf,最后将最后80个字节置为空格

### Exercise9

    "0xF0100034 movl $(bootstacktop),%esp"  处开辟栈,栈位置从 0xF0110000 到 0xF0108000
        #define KSTKSIZE    (8*PGSIZE(4096))          // size of a kernel stack

### Exercise10
    首先压入EBP 然后压入EBX保存寄存器 最后leave ret 恢复栈帧

### Exercise11 
    ......

### Exercise12
    [摘抄](https://www.jianshu.com/p/84f62a05a7e6)
    int mon_backtrace(int argc, char **argv, struct Trapframe *tf)
    {
        uint32_t ebp, *ptr_ebp;
        struct Eipdebuginfo info;
        ebp = read_ebp();
        cprintf("Stack backtrace:\n");
        while (ebp != 0) {
            ptr_ebp = (uint32_t *)ebp;
            cprintf("\tebp %x  eip %x  args %08x %08x %08x %08x %08x\n", ebp, ptr_ebp[1], ptr_ebp[2], ptr_ebp[3], ptr_ebp[4], ptr_ebp[5], ptr_ebp[6]);
            if (debuginfo_eip(ptr_ebp[1], &info) == 0) {
                uint32_t fn_offset = ptr_ebp[1] - info.eip_fn_addr;
                cprintf("\t\t%s:%d: %.*s+%d\n", info.eip_file, info.eip_line,info.eip_fn_namelen,  info.eip_fn_name, fn_offset);
            }
            ebp = *ptr_ebp;
        }
        return 0;
    }
