#### 结构

```
struct proc_struct {
    enum proc_state state; 	// 此进程状态
    int pid; 				// 此进程ID
    int runs; 				// 此进程运行次数
    uintptr_t kstack; 		// 此进程独有的内核栈
    volatile bool need_resched; // 是否想要重新配置从而释放CPU?,若为1,则不会占用CPU,从而CPU可以执行其他进程
    struct proc_struct *parent; // 创建此线程的父线程
    struct mm_struct *mm; 		// 此进程内存管理的信息
    struct context context; 	// 调用此进程时,从保存执行现场中获取信息继续执行此进程
    struct trapframe *tf; 		// 陷入中断时,用于保存寄存器等值的结构
    uintptr_t cr3; 				// 项目录表的物理地址
    uint32_t flags; 			// 此进程标志位
    char name[PROC_NAME_LEN + 1]; 	// 进程名称
    list_entry_t list_link; 		// 所有进程结构体都将链入此链表,链表头proc_list
    list_entry_t hash_link; 		// 所哟进程结构中hash_link将基于pid链接入hash_list[HASH_LIST_SIZE]中
};

static struct proc *initproc; 	//指向一个内核线程
static struct proc *current;	//当前占用CPU且处于运行状态的进程控制块指针,通常只读,在进程切换时才作更改
```

#### 过程分析

#### proc_init

```
void proc_init(void) {
    int i;

    list_init(&proc_list); //初始化用于所有proc_struct中list_link的链表头
    for (i = 0; i < HASH_LIST_SIZE; i ++) { //初始化哈希表
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL) { //为idleproc的proc_struct申请一段内存空间
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0; //进程ID置0
    idleproc->state = PROC_RUNNABLE; //进程状态置为就绪状态
    idleproc->kstack = (uintptr_t)bootstack; //内核栈设置为ucore启动时的堆栈
    idleproc->need_resched = 1; //置1,调用schedule时,会将idleproc从CPU中释放,从而将进程让给需要执行的进程
    set_proc_name(idleproc, "idle"); //设置进程名称为idle
    nr_process ++; //进程计数 +1

    current = idleproc; //当前进程指针赋值给current

    int pid = kernel_thread(init_main, "Hello world!!", 0); // 进程参数设置
    if (pid <= 0) {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid); // 通过返回的pid值 寻找对应的proc_struct结构体位置
    set_proc_name(initproc, "init"); // 设置进程名称为 init

    assert(idleproc != NULL && idleproc->pid == 0); //简单的判断
    assert(initproc != NULL && initproc->pid == 1);
}
```

#### alloc_proc

```
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
    proc->state = PROC_UNINIT; // 设置进程为"初始"态
    proc->pid = -1; // 设置进程pid的未初始化值
    proc->runs = 0;
    proc->kstack = 0;
    proc->need_resched = 0; // 进程为可占用CPU的状态
    proc->parent = NULL;
    proc->mm = NULL
    memset(&(proc->context), 0, sizeof(struct context));
    proc->tf = NULL;
    proc->cr3 = boot_cr3; //使用内核页目录表的基址
    proc->flags = 0;
    memset(proc->name, 0, PROC_NAME_LEN);
    }
    return proc;
}
```

#### kernel_thread函数

```
int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe)); //trapframe结构的临时中断帧tf清空
    tf.tf_cs = KERNEL_CS; // CS设置为KERNEL_CS
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS; // 同样设置为 内核态的 KERNEL_DS
    tf.tf_regs.reg_ebx = (uint32_t)fn; // 调用init_main函数的指针交给ebx寄存器
    tf.tf_regs.reg_edx = (uint32_t)arg;// 调用init_main函数的参数交给edx
    tf.tf_eip = (uint32_t)kernel_thread_entry; //汇编指令,其实就是调用init_main
    return do_fork(clone_flags | CLONE_VM, 0, &tf); // 调用do_fork,将临时中断帧交给新申请的一段 2*PGSIZE的栈
}
```

##### kernel_thread_entry

```
.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)

    pushl %edx              # push arg
    call *%ebx              # call fn

    pushl %eax              # save the return value of fn(arg)
    call do_exit            # call do_exit to terminate current thread
用于调用initmain,然后结束进程
```

#### do_fork

```
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) { //
        goto fork_out;
    }
    ret = -E_NO_MEM;
    if ((proc = alloc_proc()) == NULL) { //申请一块内存用于放置管理此进程信息的proc_struct结构体
        goto fork_out;
    }

    proc->parent = current; // 父进程赋值为current,此处为adle_proc

    if (setup_kstack(proc) != 0) { //申请一块2*PGSZIE内存用于进程堆栈
        goto bad_fork_cleanup_proc;
    }
    if (copy_mm(clone_flags, proc) != 0) { // 不清楚 这是设置mm变量的指向吗?指向当前页目录表的mm_struct结构体？
        goto bad_fork_cleanup_kstack;
    }
    copy_thread(proc, stack, tf); //将kernel_thread中的临时栈进行拷贝到新申请的栈中,并设置context等变量

    bool intr_flag;
    local_intr_save(intr_flag); // 允许中断
    {
        proc->pid = get_pid();
        hash_proc(proc);
        list_add(&proc_list, &(proc->list_link)); //将此进程链入 proc_list链表中
        nr_process ++;
    }
    local_intr_restore(intr_flag);

    wakeup_proc(proc);
    /*
    void
    wakeup_proc(struct proc_struct *proc) {
        assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
        proc->state = PROC_RUNNABLE; //设置进程为就绪状态
    }
    */

    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

##### setup_kstack

```
static int
setup_kstack(struct proc_struct *proc) {
    struct Page *page = alloc_pages(KSTACKPAGE); //申请 8K的内存用于进程堆栈
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}
```

##### copy_mm

```
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    assert(current->mm == NULL);
    /* do nothing in this project */
    return 0;
}
```

##### get_pid

```
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++ last_pid >= MAX_PID) { //判断MAX_PID是否为-1?
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list) { //遍历proc_list双向链表,寻找一个未被使用的PID,
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}
```

##### hash_proc && pid_hashfn

```
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))
#define GOLDEN_RATIO_PRIME_32       0x9e370001UL
uint32_t
hash32(uint32_t val, unsigned int bits) {
    uint32_t hash = val * GOLDEN_RATIO_PRIME_32;
    return (hash >> (32 - bits));
}
static void
hash_proc(struct proc_struct *proc) {
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link)); //将此进程链入hash_list表中
}
```

#### copy_thread

```
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1; //在申请的2*PGSIZE的新进程的内核栈的顶部开辟一段内存储存中断帧参数
    *(proc->tf) = *tf; // 将临时中断帧中的参数拷贝到上述空间处
    proc->tf->tf_regs.reg_eax = 0; // eax置0
    proc->tf->tf_esp = esp;		   // esp 指向当前传入的参数的esp
    proc->tf->tf_eflags |= FL_IF;  // 设置标志位 为进程能够中断

    proc->context.eip = (uintptr_t)forkret; // 中断时储存的下一条指令的地址
    proc->context.esp = (uintptr_t)(proc->tf); // 中断时储存的栈帧位置,因为有上述的中断帧占据了部分位置,故只能将esp设置为中断帧的起始位置
}
```

###### init.c 中kern_init函数 在完成了进程初始化之后,调用cpu_idle寻找进程然后执行

#### schedule

```
void
schedule(void) {
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        // 遍历proc_struct结构体中list_link双向链表,寻找到一个进程是处于就绪状态
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE) {
                    break;
                }
            }
        } while (le != last);
        if (next == NULL || next->state != PROC_RUNNABLE) {
            next = idleproc;
        }
        next->runs ++; // 运行计数 + 1
        if (next != current) {
            proc_run(next); //调用proc_run()函数,传入该进程的proc_struct结构指针
        }
    }
    local_intr_restore(intr_flag);
}
```

#### proc_run

```
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag); //设置flag为 允许中断,最终是执行了sti指令
        {
            current = proc; // 当前的进程修改为传入的进程指针
            load_esp0(next->kstack + KSTACKSIZE); //设置TSS信息即ts结构中esp0值为当前进程的内核栈地址,此处是特权级权限切换才会用到
            lcr3(next->cr3); // 设置当前进程的proc_struct结构中 cr3值为当前进程的一级页表地址,完成页表切换
            switch_to(&(prev->context), &(next->context)); //于switch_to函数实现进程的切换
        }
        local_intr_restore(intr_flag);// 屏蔽中断,实现为 cli指令
    }
}
```

#### switch_to

```
// context结构体成员
struct context {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};
```

```
.text
.globl switch_to
switch_to:                      # switch_to(from, to)

    # save from's registers
    movl 4(%esp), %eax          # eax points to from
    popl 0(%eax)                # save eip !popl
    movl %esp, 4(%eax)          # save esp::context of from
    movl %ebx, 8(%eax)          # save ebx::context of from
    movl %ecx, 12(%eax)         # save ecx::context of from
    movl %edx, 16(%eax)         # save edx::context of from
    movl %esi, 20(%eax)         # save esi::context of from
    movl %edi, 24(%eax)         # save edi::context of from
    movl %ebp, 28(%eax)         # save ebp::context of from

    # restore to's registers
    movl 4(%esp), %eax          # not 8(%esp): popped return address already
                                # eax now points to to
    movl 28(%eax), %ebp         # restore ebp::context of to
    movl 24(%eax), %edi         # restore edi::context of to
    movl 20(%eax), %esi         # restore esi::context of to
    movl 16(%eax), %edx         # restore edx::context of to
    movl 12(%eax), %ecx         # restore ecx::context of to
    movl 8(%eax), %ebx          # restore ebx::context of to
    movl 4(%eax), %esp          # restore esp::context of to

    pushl 0(%eax)               # push eip

    ret
// 简单的 将 寄存器的值进行切换 ret返回即为proc->context.eip = (uintptr_t)forkret; 调用了forkret函数
```

#### forkret

```
struct trapframe {
    {
    uint32_t reg_edi;
    uint32_t reg_esi;
    uint32_t reg_ebp;
    uint32_t reg_oesp;          /* Useless */
    uint32_t reg_ebx;
    uint32_t reg_edx;
    uint32_t reg_ecx;
    uint32_t reg_eax;
	};
    uint16_t tf_gs;
    uint16_t tf_padding0;
    uint16_t tf_fs;
    uint16_t tf_padding1;
    uint16_t tf_es;
    uint16_t tf_padding2;
    uint16_t tf_ds;
    uint16_t tf_padding3;
    uint32_t tf_trapno;
    /* below here defined by x86 hardware */
    uint32_t tf_err;
    uintptr_t tf_eip;
    uint16_t tf_cs;
    uint16_t tf_padding4;
    uint32_t tf_eflags;
    /* below here only when crossing rings, such as from user to kernel */
    uintptr_t tf_esp;
    uint16_t tf_ss;
    uint16_t tf_padding5;
}
```



```
.globl __trapret
__trapret:
    # restore registers from stack
    popal //将中断帧的数据 弹出到 当前进程的寄存器中

    # restore %ds, %es, %fs and %gs
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # get rid of the trap number and error code
    addl $0x8, %esp // 跳过tf_trapno 与 tf_err值
    iret // 恢复eip cs eflags esp ss,调用了init_main函数,打印

.globl forkrets
forkrets:
    # set stack to this new process's trapframe
    movl 4(%esp), %esp # 令 当前esp寄存器指向proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;时开辟的中断帧
    jmp __trapret //跳转
```