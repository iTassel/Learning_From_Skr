后面的实在不知道怎么写笔记,只能分析ucore的代码了
lab5的主要分析在proc.c和pmm.c中,以及/user中

lab5 主要实现了对拥护态进程的创建与执行,主要修改代码依旧位于process/proc.c中
从lab4中可以发现,lab5中的init_main()函数作出了修改
在init_main函数中又创建了一个新的进程,即调用user_main函数
而user_main函数经kernel_execve实现,最终实现是通过do_execve(/kern/syscall/syscall.c中有对sys_execve的实现函数)
具体笔记实在写不出来,有一个BUG一直没找到,就是init_main函数已经通过了,但是在do_exit结束进程的时候
    if (current == initproc) {
        panic("initproc exit.\n");
    }
这里始终会报错,一直没找到原因,希望其他师傅能够帮忙找找

其实主要的内核实现代码位于/kern/文件夹中,/user/是一些用户态的功能的一些库或者其他的,感觉关联不大,ucorebook里面有个地方讲到了修改makefile文件,但是我也没找到地方,我太菜了......
