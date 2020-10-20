//gcc src.c -o main
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<malloc.h>
#include<elf.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define Dynsym 0
#define Symtab 1
void print_Sym(const u_char *ELF_buf)
{
	size_t SHT_offset;
	size_t i, j;
	size_t n;
	size_t count[2];
	Elf64_Ehdr *EHdr;
	Elf64_Shdr *SecHdr;
	Elf64_Shdr *SecHdr_Entry;
	Elf64_Shdr *_SecHdr_DynSym = NULL;
	Elf64_Shdr *_SecHdr_Symtab = NULL;
	Elf64_Sym  *SymHdr;
	u_char *Sec_Name_Table;
	u_char *SymStrTable;
	u_char *DynStrTable;
	EHdr = (Elf64_Ehdr*)ELF_buf;
	n =  EHdr->e_shnum;
	SHT_offset = EHdr->e_shoff;
	SecHdr_Entry = (Elf64_Shdr*)(ELF_buf + SHT_offset +  sizeof(Elf64_Shdr) * (EHdr->e_shstrndx )); //Entry Section Header
	Sec_Name_Table = (u_char *)(ELF_buf + SecHdr_Entry->sh_offset); //Section item name
	for (i = 0; i < n; ++i)
	{
		SecHdr = (Elf64_Shdr*)(ELF_buf + SHT_offset +  i * sizeof(Elf64_Shdr));
		// GET the .strtab address
		if (!strcmp( Sec_Name_Table+SecHdr->sh_name, ".strtab"))
		{
			SymStrTable = (u_char *)(ELF_buf + SecHdr->sh_offset); //GET the symbols Name
		}
		else if(!strcmp( Sec_Name_Table+SecHdr->sh_name, ".dynstr"))
		{
			DynStrTable = (u_char *)(ELF_buf + SecHdr->sh_offset);
		}
		else if(!strcmp( Sec_Name_Table+SecHdr->sh_name, ".dynsym"))
		{
			_SecHdr_DynSym = (Elf64_Shdr *)SecHdr;
			count[Dynsym] =  (_SecHdr_DynSym->sh_size / _SecHdr_DynSym->sh_entsize);
		}
		else if(!strcmp( Sec_Name_Table+SecHdr->sh_name, ".symtab"))
		{
			_SecHdr_Symtab = (Elf64_Shdr *)SecHdr;
			count[Symtab] =  (_SecHdr_Symtab->sh_size / _SecHdr_Symtab->sh_entsize);
		}
	}
	
	if(_SecHdr_DynSym)
	{
		printf("Symbol table '.dynsym' contains %d entries:\n",count[Dynsym]);
		printf("\tNum:\tValue\t\tSize\t Name\n");
		for ( j = 0; j < count[Dynsym]; ++j)
		{
			SymHdr = (Elf64_Sym*)(ELF_buf + _SecHdr_DynSym->sh_offset + j * (sizeof(Elf64_Sym)));
			printf("\t%-2d: ",j);
			printf(" %#016x\t",SymHdr->st_value );
			printf("%04x\t",SymHdr->st_size );
			printf(" %s\n",DynStrTable+SymHdr->st_name );
		}
	}
	
	if(_SecHdr_Symtab)
	{
		printf("Symbol table '.symtab' contains %d entries:\n",count[Symtab]);
		printf("\tNum:\tValue\t\tSize\t Name\n");
		for ( j = 0; j < count[Symtab]; ++j)
		{
			SymHdr = (Elf64_Sym*)(ELF_buf + _SecHdr_Symtab->sh_offset + j * (sizeof(Elf64_Sym)));
			printf("\t%-2d: ",j);
			printf(" %#016x\t",SymHdr->st_value );
			printf("%04x\t",SymHdr->st_size );
			printf("%s\n",SymStrTable+SymHdr->st_name );
		}
	}
	

}

void usage()
{
	fprintf(stderr,"Follow is the usages:\n");
	fprintf(stderr,"\t-s > print all symbols in terminal\n");
	exit(0);
}
void init_IO()
{
	setvbuf(stdin,0LL,2,0LL);
	setvbuf(stdout,0LL,2,0LL);
	setvbuf(stderr,0LL,2,0LL);
}
int main(int argc,char *argv[])
{
	init_IO();
	FILE *stream = fopen(argv[2],"rb");
	if(!stream)
	{
		fprintf(stderr,"File Not Found :(\n");
		usage();
	}
	fseek(stream,0,SEEK_END); // ** GET FILE_SIZE
	size_t filesz = ftell(stream);
	fseek(stream,0,SEEK_SET);
	char *buf = calloc(filesz + 0x10,1);
	fread(buf,filesz,1,stream);
	fclose(stream);
	if(memcmp(buf,"\177ELF",4))
	{
		fprintf(stderr,"ELF Magic Not Found :(\n");
		exit(0);
	}
	char version = buf[4];
	if(version == 2)
	{
		fprintf(stdout,"File class: 64-bit :)\n");
		struct Elf64_Ehdr *Ehdr = (struct Elf64_Ehdr *)calloc(sizeof(Elf64_Ehdr),1);
		memcpy((char*)Ehdr,buf,sizeof(Elf64_Ehdr));
		
	}
	else if(version == 1)
	{
		fprintf(stdout,"File class: 32-bit :)\n");
		struct Elf32_Ehdr *Ehdr = (struct Elf32_Ehdr *)calloc(sizeof(Elf32_Ehdr),1);
		memcpy((char*)Ehdr,buf,sizeof(Elf32_Ehdr));
	}
	else
	{
		fprintf(stderr,"File Class Not Found :(\n");
		exit(0);
	}
	char op = *((char*)strstr(argv[1],"-") + 1);
	switch(op)
	{
		case 's':
			print_Sym(buf);
			break;
		default:
			usage();	
	}
}
