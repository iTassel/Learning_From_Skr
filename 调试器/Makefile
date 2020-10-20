objects = src.o linenoise.o
all:clean ${objects}
	g++ -o main ${objects} /usr/lib/libelf++.so.0 /usr/lib/libdwarf++.so.0 -g
src.o:
	cc -c src.cpp
linenoise.o:
	cc -c linenoise.c
clean:
	$(RM) main ${objects}
.PHONY: all clean

