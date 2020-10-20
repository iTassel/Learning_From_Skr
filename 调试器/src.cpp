#include <iostream>
#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <elf.h>
#include <vector>
#include <iterator>
#include <unistd.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <fcntl.h>
#include <elf/elf++.hh>
#include <dwarf/dwarf++.hh>
#include "linenoise.h"

enum class reg {
    rax, rbx, rcx, rdx,
    rdi, rsi, rbp, rsp,
    r8,  r9,  r10, r11,
    r12, r13, r14, r15,
    rip, rflags,    cs,
    orig_rax, fs_base,
    gs_base,
    fs, gs, ss, ds, es
};

constexpr std::size_t n_registers = 27;

struct reg_descriptor {
    reg r;
    int dwarf_r;
    std::string name;
};

const std::array<reg_descriptor, n_registers> g_register_descriptors {{
    { reg::r15, 15, "r15" },
    { reg::r14, 14, "r14" },
    { reg::r13, 13, "r13" },
    { reg::r12, 12, "r12" },
    { reg::rbp, 6, "rbp" },
    { reg::rbx, 3, "rbx" },
    { reg::r11, 11, "r11" },
    { reg::r10, 10, "r10" },
    { reg::r9, 9, "r9" },
    { reg::r8, 8, "r8" },
    { reg::rax, 0, "rax" },
    { reg::rcx, 2, "rcx" },
    { reg::rdx, 1, "rdx" },
    { reg::rsi, 4, "rsi" },
    { reg::rdi, 5, "rdi" },
    { reg::orig_rax, -1, "orig_rax" },
    { reg::rip, -1, "rip" },
    { reg::cs, 51, "cs" },
    { reg::rflags, 49, "eflags" },
    { reg::rsp, 7, "rsp" },
    { reg::ss, 52, "ss" },
    { reg::fs_base, 58, "fs_base" },
    { reg::gs_base, 59, "gs_base" },
    { reg::ds, 53, "ds" },
    { reg::es, 50, "es" },
    { reg::fs, 54, "fs" },
    { reg::gs, 55, "gs" },
}};

enum class symbol_type {
    notype,            // No type (e.g., absolute symbol)
    object,            // Data object
    func,              // Function entry point
    section,           // Symbol is associated with a section
    file,              // Source file associated with the
};                     // object file

std::string to_string (symbol_type st) {
    switch (st) {
    case symbol_type::notype: return "notype";
    case symbol_type::object: return "object";
    case symbol_type::func: return "func";
    case symbol_type::section: return "section";
    case symbol_type::file: return "file";
    }
    
}

struct symbol {
    symbol_type type;
    std::string name;
    std::uintptr_t addr;
};


std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> out{};
    std::stringstream ss {s};
    std::string item;

    while (std::getline(ss,item,delimiter)) {
        out.push_back(item);
    }

    return out;
}

bool is_suffix(const std::string& s, const std::string& of) {
    if (s.size() > of.size()) return false;
    auto diff = of.size() - s.size();
    return std::equal(s.begin(), s.end(), of.begin() + diff);
}

bool is_prefix(const std::string& s, const std::string& of) {
    if (s.size() > of.size()) return false;
    return std::equal(s.begin(), s.end(), of.begin());
}

uint64_t get_register_value(pid_t pid, reg r) {
    user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),[r](auto&& rd) { return rd.r == r; });

    return *(reinterpret_cast<uint64_t*>(&regs) + (it - begin(g_register_descriptors)));
}

void set_register_value(pid_t pid, reg r, uint64_t value) {
    user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),[r](auto&& rd) { return rd.r == r; });

    *(reinterpret_cast<uint64_t*>(&regs) + (it - begin(g_register_descriptors))) = value;
    ptrace(PTRACE_SETREGS, pid, nullptr, &regs);
}

uint64_t get_register_value_from_dwarf_register (pid_t pid, unsigned regnum) {
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),[regnum](auto&& rd) { return rd.dwarf_r == regnum; });
    if (it == end(g_register_descriptors)) {
        throw std::out_of_range{"Unknown dwarf register"};
    }

    return get_register_value(pid, it->r);
}

std::string get_register_name(reg r) {
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),[r](auto&& rd) { return rd.r == r; });
    return it->name;
}

reg get_register_from_name(const std::string& name) {
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),[name](auto&& rd) { return rd.name == name; });
    return it->r;
}

class breakpoint {
	public:
		breakpoint() {
			std::cout << "Instruction Have Saved" <<std::endl;
		}
		breakpoint(pid_t pid,std::intptr_t addr) 
			: m_pid{pid},m_addr{addr},m_enabled{false},m_saved_data{}
		{}
		void enable();
		void disable();
		auto is_enabled() const -> bool{return m_enabled;}
		auto get_address() const -> std::intptr_t{ return m_addr;}
	private:
		pid_t m_pid;
		std::intptr_t m_addr;
		bool m_enabled;
		uint8_t m_saved_data;
};

class Fdbg {
	public:
		Fdbg(std::string prog_name , pid_t pid) 
			: m_prog_name{std:: move(prog_name)},m_pid{pid} {
		    auto fd = open(m_prog_name.c_str(), O_RDONLY);

		    m_elf = elf::elf{elf::create_mmap_loader(fd)};	
		    m_dwarf = dwarf::dwarf{dwarf::elf::create_loader(m_elf)};
		}
		void run();
		void handle_command(const std::string& line);
		void continue_execution();
		void set_breakpoint_at_address(std::intptr_t addr);
		void dump_registers();
		uint64_t read_memory(uint64_t address);
		void write_memory(uint64_t address, uint64_t value);
		uint64_t get_pc ();
		void set_pc(uint64_t pc);
		void step_over_breakpoint();
		void wait_for_signal();
		dwarf::die get_function_from_pc(uint64_t pc);
		dwarf::line_table::iterator get_line_entry_from_pc(uint64_t pc);
		void print_source(const std::string& file_name,unsigned line , unsigned n_lines_context = 2);
		siginfo_t get_signal_info();
		void handle_sigtrap(siginfo_t info);
		void single_step_instruction();
		void single_step_instruction_with_breakpoint_check();
		void step_out();
		void step_in();
		void remove_breakpoint(std::intptr_t addr);
		void step_over();
		void set_breakpoint_at_function(const std::string& name);
		void set_breakpoint_at_source_line(const std::string& file, unsigned line);
		std::vector<symbol> lookup_symbol(const std::string& name);
	private:
		std::string m_prog_name;
		pid_t m_pid;
		std::unordered_map<std::intptr_t,breakpoint> m_breakpoints;
		dwarf::dwarf m_dwarf;
		elf::elf m_elf;

};


void Fdbg::single_step_instruction() {
    ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
    wait_for_signal();
}

void Fdbg::single_step_instruction_with_breakpoint_check() {
    //first, check to see if we need to disable and enable a breakpoint
    if (m_breakpoints.count(get_pc())) {
        step_over_breakpoint();
    }
    else {
        single_step_instruction();
    }
}

void Fdbg::step_out() {
    auto frame_pointer = get_register_value(m_pid, reg::rbp);
    auto return_address = read_memory(frame_pointer+8);

    bool should_remove_breakpoint = false;
    if (!m_breakpoints.count(return_address)) {
        set_breakpoint_at_address(return_address);
        should_remove_breakpoint = true;
    }

    continue_execution();

    if (should_remove_breakpoint) {
        remove_breakpoint(return_address);
    }
}


void Fdbg::step_in() {
   auto line = get_line_entry_from_pc(get_pc())->line;

    while (get_line_entry_from_pc(get_pc())->line == line) {
        single_step_instruction_with_breakpoint_check();
    }

    auto line_entry = get_line_entry_from_pc(get_pc());
    print_source(line_entry->file->path, line_entry->line);
}

void Fdbg::step_over() {
    auto func = get_function_from_pc(get_pc());
    auto func_entry = at_low_pc(func);
    auto func_end = at_high_pc(func);

    auto line = get_line_entry_from_pc(func_entry);
    auto start_line = get_line_entry_from_pc(get_pc());

    std::vector<std::intptr_t> to_delete{};

    while (line->address < func_end) {
        if (line->address != start_line->address && !m_breakpoints.count(line->address)) {
            set_breakpoint_at_address(line->address);
            to_delete.push_back(line->address);
        }
        ++line;
    }

    auto frame_pointer = get_register_value(m_pid, reg::rbp);
    auto return_address = read_memory(frame_pointer+8);
    if (!m_breakpoints.count(return_address)) {
        set_breakpoint_at_address(return_address);
        to_delete.push_back(return_address);
    }

    continue_execution();

    for (auto addr : to_delete) {
        remove_breakpoint(addr);
    }
}

void Fdbg::remove_breakpoint(std::intptr_t addr) {
    if (m_breakpoints.at(addr).is_enabled()) {
        m_breakpoints.at(addr).disable();
    }
    m_breakpoints.erase(addr);
}

dwarf::die Fdbg::get_function_from_pc(uint64_t pc) {
    for (auto &cu : m_dwarf.compilation_units()) {
        if (die_pc_range(cu.root()).contains(pc)) {
            for (const auto& die : cu.root()) {
                if (die.tag == dwarf::DW_TAG::subprogram) {
                    if (die_pc_range(die).contains(pc)) {
                        return die;
                    }
                }
            }
        }
    }

    throw std::out_of_range{"Cannot find function"};
}

dwarf::line_table::iterator Fdbg::get_line_entry_from_pc(uint64_t pc) {
    for (auto &cu : m_dwarf.compilation_units()) {
        if (die_pc_range(cu.root()).contains(pc)) {
            auto &lt = cu.get_line_table();
            auto it = lt.find_address(pc);
            if (it == lt.end()) {
                throw std::out_of_range{"Cannot Find Line Entry :("};
            }
            else {
                return it;
            }
        }
    }

	throw std::out_of_range{"Cannot Find Line Entry :("};
}

void Fdbg::set_breakpoint_at_function(const std::string& name) {
    for (const auto& cu : m_dwarf.compilation_units()) {
        for (const auto& die : cu.root()) {
            if (die.has(dwarf::DW_AT::name) && at_name(die) == name) {
                auto low_pc = at_low_pc(die);
                auto entry = get_line_entry_from_pc(low_pc);
                ++entry; //跳过函数序言
                set_breakpoint_at_address(entry->address);
            }
        }
    }
}

void Fdbg::set_breakpoint_at_source_line(const std::string& file, unsigned line) {
    for (const auto& cu : m_dwarf.compilation_units()) {
        if (is_suffix(file, at_name(cu.root()))) {
            const auto& lt = cu.get_line_table();

            for (const auto& entry : lt) {
                if (entry.is_stmt && entry.line == line) {
                    set_breakpoint_at_address(entry.address);
                    return;
                }
            }
        }
    }
}

symbol_type to_symbol_type(elf::stt sym) {
    switch (sym) {
    case elf::stt::notype: return symbol_type::notype;
    case elf::stt::object: return symbol_type::object;
    case elf::stt::func: return symbol_type::func;
    case elf::stt::section: return symbol_type::section;
    case elf::stt::file: return symbol_type::file;
    default: return symbol_type::notype;
    }
};

std::vector<symbol> Fdbg::lookup_symbol(const std::string& name) {
    std::vector<symbol> syms;

    for (auto &sec : m_elf.sections()) {
        if (sec.get_hdr().type != elf::sht::symtab && sec.get_hdr().type != elf::sht::dynsym)
            continue;

        for (auto sym : sec.as_symtab()) {
            if (sym.get_name() == name) {
                auto &d = sym.get_data();
                syms.push_back(symbol{to_symbol_type(d.type()), sym.get_name(), d.value});
            }
        }
    }

    return syms;
}
void Fdbg::print_source(const std::string& file_name, unsigned line, unsigned n_lines_context) {
    std::ifstream file {file_name};

    auto start_line = line <= n_lines_context ? 1 : line - n_lines_context;
    auto end_line = line + n_lines_context + (line < n_lines_context ? n_lines_context - line : 0) + 1;

    char c{};
    auto current_line = 1u;
    while (current_line != start_line && file.get(c)) {
        if (c == '\n') {
            ++current_line;
        }
    }
    std::cout << (current_line==line ? "> " : "  ");

    while (current_line <= end_line && file.get(c)) {
        std::cout << c;
        if (c == '\n') {
            ++current_line;
            std::cout << (current_line==line ? "> " : "  ");
        }
    }
    std::cout << std::endl;
}

siginfo_t Fdbg::get_signal_info() {
    siginfo_t info;
    ptrace(PTRACE_GETSIGINFO, m_pid, nullptr, &info);
    return info;
}
uint64_t Fdbg::get_pc() {
    return get_register_value(m_pid, reg::rip);
}

void Fdbg::set_pc(uint64_t pc) {
    set_register_value(m_pid, reg::rip, pc);
}

void Fdbg::run()
{
	int wait_status;
	auto options = 0;
	waitpid(m_pid,&wait_status,options);
	char *line = nullptr;
	while((line = linenoise("Fdbg> ")) != nullptr) {
		handle_command(line);
		linenoiseHistoryAdd(line);
		linenoiseFree(line);
	}
}
void Fdbg::handle_command(const std::string& line)
{
	if(!is_prefix(line,"\n")) {
		auto args = split(line,' ');
		auto command = args[0];
		if(is_prefix(command,"continue")) {
			continue_execution();
		}
		else if(is_prefix(command,"break") || is_prefix(command,"b")) {
		    if (args[1][0] == '0' && args[1][1] == 'x') {
		        std::string addr {args[1], 2};
		        set_breakpoint_at_address(std::stol(addr, 0, 16));
		    }
		    else if (args[1].find(':') != std::string::npos) {
		        auto file_and_line = split(args[1], ':');
		        set_breakpoint_at_source_line(file_and_line[0], std::stol(file_and_line[1],0,10));
		    }
		    else {
		        set_breakpoint_at_function(args[1]);
		    }
		}
		else if (is_prefix(command, "register"))
		{
		    if (is_prefix(args[1], "dump")) {
		        dump_registers();
		    }
		    else if (is_prefix(args[1], "read")) {
		        std::cout << get_register_value(m_pid, get_register_from_name(args[2])) << std::endl;
		    }
		    else if (is_prefix(args[1], "write")) {
		        std::string val {args[3], 2}; //assume 0xVAL
		        set_register_value(m_pid, get_register_from_name(args[2]), std::stol(val, 0, 16));
		    }
		}
		else if(is_prefix(command, "memory")) {
		    std::string addr {args[2], 2}; //assume 0xADDRESS

		    if (is_prefix(args[1], "read")) {
		        std::cout << std::hex << read_memory(std::stol(addr, 0, 16)) << std::endl;
		    }
		    if (is_prefix(args[1], "write")) {
		        std::string val {args[3], 2}; //assume 0xVAL
		        write_memory(std::stol(addr, 0, 16), std::stol(val, 0, 16));
		    }
		}
		else if(is_prefix(command, "stepi") || is_prefix(command, "si")) {
			single_step_instruction_with_breakpoint_check();
			auto line_entry = get_line_entry_from_pc(get_pc());
			print_source(line_entry->file->path, line_entry->line);
		}
		else if(is_prefix(command, "step") || is_prefix(command, "s")) {
			step_in();
		}
		else if(is_prefix(command, "next") || is_prefix(command, "n")) {
			step_over();
		}
		else if(is_prefix(command, "finish") || is_prefix(command, "fin")) {
			step_out();
		}
		else if(is_prefix(command, "symbol")) {
			auto syms = lookup_symbol(args[1]);
			for (auto&& s : syms) {
				std::cout << s.name << ' ' << to_string(s.type) << " 0x" << std::hex << s.addr << std::endl;
			}
		}
		else if (is_prefix(command, "quit") || is_prefix(command, "q") || is_prefix(command, "exit"))
		{
			std::cout << "Good Bye~" << std::endl;
			exit(0);
		}
		else 
		{
			std::cerr << "Unknown Command: " << command << " :("<<std::endl;
		}
	}
}
void Fdbg::set_breakpoint_at_address(std::intptr_t addr)
{
	std::cout << "Set breakpoint at address 0x" << std::hex << addr << std::endl;
	breakpoint bp{m_pid, addr};
	bp.enable();
	m_breakpoints[addr] = bp;
}
void Fdbg::continue_execution()
{
	step_over_breakpoint();
	ptrace(PTRACE_CONT,m_pid,nullptr,nullptr);
	wait_for_signal();
}

void Fdbg::dump_registers()	{

	for (const auto& rd : g_register_descriptors) {
        std::cout << rd.name << " 0x"
                  << std::setfill('0') << std::setw(16) 
                  << std::hex << get_register_value(m_pid, rd.r) << std::endl;
    }
}

uint64_t Fdbg::read_memory(uint64_t address) {
    return ptrace(PTRACE_PEEKDATA, m_pid, address, nullptr);
}

void Fdbg::write_memory(uint64_t address, uint64_t value) {
    ptrace(PTRACE_POKEDATA, m_pid, address, value);
}

void breakpoint::enable()
{
	auto data = ptrace(PTRACE_PEEKDATA,m_pid,m_addr,nullptr);
	m_saved_data = static_cast<uint8_t>(data & 0xFF); // save
	uint64_t int3 = 0xCC;
	uint64_t data_with_int3 = ((data & ~0xFF) | int3);
	ptrace(PTRACE_POKEDATA,m_pid,m_addr,data_with_int3);
	
	m_enabled = true;
}
void breakpoint::disable()
{
	auto data = ptrace(PTRACE_PEEKDATA,m_pid,m_addr,nullptr);
	auto restored_data = ((data & ~0xFF) | m_saved_data);
	ptrace(PTRACE_POKEDATA,m_pid,m_addr,restored_data);
	m_enabled = false;
}
void Fdbg::handle_sigtrap(siginfo_t info) {
	switch(info.si_code) {
		case SI_KERNEL:
		case TRAP_BRKPT:
		{
			set_pc(get_pc() -1);
			std::cout << "Hit Breakpoint at address 0x" << std::hex << get_pc() << std::endl;
			auto line_entry = get_line_entry_from_pc(get_pc());
			print_source(line_entry->file->path,line_entry->line);
			return ;
		}
		case TRAP_TRACE:
			return ;
		default:
			std::cout << "Unknown SIGTRAP code " << info.si_code << std::endl;
			return ;
	}
}

void Fdbg::step_over_breakpoint() {

    if (m_breakpoints.count(get_pc())) {
        auto& bp = m_breakpoints[get_pc()];

        if (bp.is_enabled()) {
            bp.disable();
            ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
            wait_for_signal();
            bp.enable();
        }
    }
}

void Fdbg::wait_for_signal() {
    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options);
    
    auto siginfo = get_signal_info();
    
    switch(siginfo.si_signo) {
    case SIGTRAP:
    	handle_sigtrap(siginfo);
    	break;
   	case SIGSEGV:
   		std::cout << "Yay,segfault. Reason " << siginfo.si_code << std::endl;
   		break;
   	default:
   		std::cout << "Got Signal " << strsignal(siginfo.si_signo) << std::endl;
    }
}
int main(int argc,char *argv[])
{
	if(argc < 2)
	{
		std::cerr << "Starting program:  "<< std::endl;
		std::cerr << "No executable file specified."<< std::endl;
		std::cerr << "Use the 'file' or 'exec-file' command." << std::endl;
		exit(0);
	}
	
	auto prog = argv[1];
	auto pid = fork();
	if(pid == 0)
	{
		ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
		execl(prog,prog,nullptr);
	}
	else if(pid >= 1)
	{
		Fdbg DBG{prog,pid};
		DBG.run();
	}
}
