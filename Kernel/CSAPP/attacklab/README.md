#### ctarget:

main -> stable_launch -> launch -> test -> getbuf -> Gets 

save_char:  gets_cnt+=1  然后从trans_char 中利用输入的字节取字符 每两个字节 一个 空格 保存在gets_buf中

而 *i = v2; 此处就是可以一直下溢到返回地址了

这个在pwn里面就是一个ret2text的作用,利用修改ret address 然后执行到touch1函数,把返回地址修改成touch1函数

然后修改成touch2 函数地址  其中有个判断传入的参数是否为cookie

所以 此处我们需要讲cookie的值0x59B997FA 交给rdi寄存器 因为C里面 64位ELF程序 是靠rdi rsi rdx顺序传参的

构造 mov rdi,0x59b997fa;push  0x4017ec;ret

因为开辟的栈区是一个确定的值且具有RWX权限,所以,直接ret rsp,然后在rsp的顶端布置好参数 即 上述汇编指令由pwntools里面的asm函数转换成汇编机器字节码

之后就是touch3 函数 此函数有个hexmatch的比较,同理,讲cookie字符串布置在栈中,然后将cookie的地址交给rdi寄存器,之后就通过strncmp的比较



#### rtarget:

因为第二个程序多了很多东西,比如没有单独开辟确定的一块空间作为栈空间,而是使用的系统分配的栈区,没有了执行的权限也不能准确定位

但此处可以利用ROP进行攻击,寻找Gadget可以利用ROPgadget此工具寻找,ROPgadget --binary ./filename --only ''  |grep 'rdi'这样构造一条筛选的命令

0x000000000040141b : pop rdi ; ret

直接将rdi寄存器的值修改为 cookie的值

然后传参完就调用 touch2函数



接下来就是利用ROP做touch3了,因为传入的是cookie的字符串指针,所以稍微麻烦一点,需要构造rdi的指针指向cookie的字符串

0x0000000000401a06 : mov rax, rsp ; ret

0x00000000004019a2 : mov rdi, rax ; ret

0x00000000004019d8 : add al, 0x37 ; ret

此处则将$rsp+0x37的位置布置成cookie的值,然后将rsp的指针交给rax,再对rax进行计算,使rax的指向为cookie的字符串,具体可以用 x/s 查看是否为cookie的字符串,然后将此rax的值交给rdi,之后顺利调用touch3函数即可