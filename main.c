/*
	Original code in C++ written by onelonecoder / onelonecoder.com
	
	https://github.com/OneLoneCoder/olcNES/tree/master/Part%232%20-%20CPU
	
	Find David on youtube: https://www.youtube.com/watch?v=8XmxKPJDGU0
	
	His tutorials have inspired me to dive lower down in languages.
	
	This C source is a nearly direct port of his code, moving from OOP to functional
	and adding some helper functions + a pseudo display device to hook into the bus.
	
	All cpu_<OPCODE> + cpu_<ADDRESSMODE> functions are basically unmodified, as well as the lookup array of INSTRUCTION structs.
	
*/

#include "main.h"

//declare the vars
	DIS dis;
	RAM ram;
	Bus bus;
	CPU cpu;

//holder for disassembler code
char code[MAX_MEM][50];

// Display range
	uint16_t displayMemLoc = 0x9000;
	uint16_t displayMemLen = 0x0130;
	uint16_t disBufferOffset = 0x0010;
	uint16_t disConsoleOffset = 0x0030;
	uint8_t disOPCode = 0x0000;

// Represents the working input value to the ALU
	uint8_t cpuFetched = 0x00;
	
// A convenience variable used everywhere
	uint16_t cpuTemp  = 0x0000;
	
// All used memory addresses end up in here, note not every cpu.PC++ writes to this var
	uint16_t cpuAddrAbs     = 0x0000;

// Represents absolute address following a branch
	uint16_t cpuAddrRel     = 0x0000;

//last opcode mem addr for helper functions
	uint16_t cpuLastOpAddr = 0x0000;

// Is the instruction byte
	uint8_t  cpuOPCode      = 0x00;
	
// Counts how many cpuCycles the instruction has remaining
	uint8_t  cpuCycles      = 0;
	
//flag to say cycles just set
	bool cuCyclesInit = false;

// A global accumulation of the number of clocks
	uint32_t cpuClockCount  = 0;
	
//flag to see if we've run through memory or not
	bool cpuExecuting = true;
	
//size of program in bytes
	long programSize = 0;
	
//start addr of program
	uint16_t programStart = 0x0000;
	
/**
* Display function opcode lookup table
*/
	INST_DIS lookupDis[256] = {
		{"NOP", "No Update", &dis_NOP,1 },                  //0x0000
		{"ACB", "Add Character to Buffer", &dis_ACB,1 },    //0x0001
		{"ANB", "Add Number to Buffer", &dis_ANB,1 },       //0x0002
		{"WRB", "Write Buffer to Console", &dis_WRB,1 },    //0x0003
		{"CLC", "Clear Console", &dis_CLC,1 },              //0x0004
		{"CLB", "Clear Buffer", &dis_CLB,1 },               //0x0005
	};
	
/**
* Assembles the translation table. It's big, it's ugly, but it yields a convenient way
* to emulate the 6502. I'm certain there are some "code-golf" strategies to reduce this
* but I've deliberately kept it verbose for study and alteration

* It is 16x16 entries. This gives 256 instructions. It is arranged to that the bottom
* 4 bits of the instruction choose the column, and the top 4 bits choose the row.

* For convenience to get function pointers to members of this class, I'm using this
* or else it will be much much larger :D

* The table is one big initialiser list of initialiser lists...
*/
	INSTRUCTION lookupCpu[256] = {
		{"BRK", "Break Interrupt, Implied", &cpu_BRK, &cpu_IMM, 7 },	                {"ORA", "Or with A, X-indexed, Indirect", &cpu_ORA, &cpu_IZX, 6 },	        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 3 },	                {"ORA", "OR with A, Zero Paged", &cpu_ORA, &cpu_ZP0, 3 },	                {"ASL", "Arithmetic Shift Left, Zero Paged", &cpu_ASL, &cpu_ZP0, 5 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },	    {"PHP", "Push Status to Stack, Implied", &cpu_PHP, &cpu_IMP, 3 },	{"ORA", "OR with A, Immediate", &cpu_ORA, &cpu_IMM, 2 },	                        {"ASL", "Arithmetic shift Left, Implied", &cpu_ASL, &cpu_IMP, 2 },	{"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	            {"ORA", "OR with A, Absolute", &cpu_ORA, &cpu_ABS, 4 },	                        {"ASL", "Arithmetic Shift Left, Absolute", &cpu_ASL, &cpu_ABS, 6 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },
		{"BPL", "Branch on Plus, Relative", &cpu_BPL, &cpu_REL, 2 },	                {"ORA", "Or with A, Y-indexed, Indirect", &cpu_ORA, &cpu_IZY, 5 },	        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	                {"ORA", "OR with A, Zero Paged, X-indexed", &cpu_ORA, &cpu_ZPX, 4 },	        {"ASL", "Arithmetic Shift Left, Zero Paged, X-indexed", &cpu_ASL, &cpu_ZPX, 6 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },	    {"CLC", "Clear Carry, Implied", &cpu_CLC, &cpu_IMP, 2 },            	{"ORA", "OR with A, Absolute, Y-indexed", &cpu_ORA, &cpu_ABY, 4 },	            {"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 2 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	            {"ORA", "OR with A, Absolute, X-indexed", &cpu_ORA, &cpu_ABX, 4 },	            {"ASL", "Arithmetic Shift Left, Absolute, X-indexed", &cpu_ASL, &cpu_ABX, 7 },	{"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },
		{"JSR", "Jump Subroutine, Absolute", &cpu_JSR, &cpu_ABS, 6 },	            {"AND", "AND, X-indexed, Indirect", &cpu_AND, &cpu_IZX, 6 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"BIT", "Bit Test, Zero Paged", &cpu_BIT, &cpu_ZP0, 3 },	            {"AND", "AND, Zero Paged", &cpu_AND, &cpu_ZP0, 3 },	                        {"ROL", "Rotate Left, Zero Paged", &cpu_ROL, &cpu_ZP0, 5 },	                        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },	    {"PLP", "Pull Status from Stack, Implied", &cpu_PLP, &cpu_IMP, 4 },	{"AND", "AND, Immediate", &cpu_AND, &cpu_IMM, 2 },	                            {"ROL", "Rotate Left, Implied", &cpu_ROL, &cpu_IMP, 2 },            	{"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	{"BIT", "Bit Test, Absolute", &cpu_BIT, &cpu_ABS, 4 },	        {"AND", "AND, Absolute", &cpu_AND, &cpu_ABS, 4 },	                            {"ROL", "Rotate Left, Absolute", &cpu_ROL, &cpu_ABS, 6 },	                    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },
		{"BMI", "Branch on Minus, Relative", &cpu_BMI, &cpu_REL, 2 },	            {"AND", "AND, Y-indexed, Indirect", &cpu_AND, &cpu_IZY, 5 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	                {"AND", "AND, Zero Paged, X-indexed", &cpu_AND, &cpu_ZPX, 4 },	            {"ROL", "Rotate Left, Zero Paged, X-indexed", &cpu_ROL, &cpu_ZPX, 6 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },	    {"SEC", "Set Carry, Implied", &cpu_SEC, &cpu_IMP, 2 },	            {"AND", "AND, Absolute, Y-indexed", &cpu_AND, &cpu_ABY, 4 },	                    {"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 2 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	            {"AND", "AND, Absolute, X-indexed", &cpu_AND, &cpu_ABX, 4 },	                    {"ROL", "Rotate Left, Absolute, X-indexed", &cpu_ROL, &cpu_ABX, 7 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },
		{"RTI", "Return from Interrupt, Implied", &cpu_RTI, &cpu_IMP, 6 },	        {"EOR", "Exclusive OR, X-indexed, Indirect", &cpu_EOR, &cpu_IZX, 6 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 3 },	                {"EOR", "Exclusivve OR, Zero Paged", &cpu_EOR, &cpu_ZP0, 3 },	            {"LSR", "Logical Shift Right, Zero Paged", &cpu_LSR, &cpu_ZP0, 5 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },	    {"PHA", "Push A to Stack, Implied", &cpu_PHA, &cpu_IMP, 3 },	        {"EOR", "Exclusive OR, Immediate", &cpu_EOR, &cpu_IMM, 2 },	                    {"LSR", "Logical shift Right, Implied", &cpu_LSR, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	{"JMP", "Jump, Absolute", &cpu_JMP, &cpu_ABS, 3 },	            {"EOR", "Exclusive OR, Absolute", &cpu_EOR, &cpu_ABS, 4 },	                    {"LSR", "Logical Shift Right, Absolute", &cpu_LSR, &cpu_ABS, 6 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },
		{"BVC", "Branch on Overflow Clear, Relative", &cpu_BVC, &cpu_REL, 2 },	    {"EOR", "Exclusive OR, y-indexed, Indirect", &cpu_EOR, &cpu_IZY, 5 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	                {"EOR", "Exclusivve OR, Zero Paged, X-indexed", &cpu_EOR, &cpu_ZPX, 4 },	    {"LSR", "Logical Shift Right, Zero Paged, X-indexed", &cpu_LSR, &cpu_ZPX, 6 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },	    {"CLI", "Clear Interrupt Disable, Implied", &cpu_CLI, &cpu_IMP, 2 },	{"EOR", "Exclusive OR, Absolute, Y-indexed", &cpu_EOR, &cpu_ABY, 4 },	        {"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 2 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	            {"EOR", "Exclusive OR, Absolute, X-indexed", &cpu_EOR, &cpu_ABX, 4 },	        {"LSR", "Logical Shift Right, Absolute, X-indexed", &cpu_LSR, &cpu_ABX, 7 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },
		{"RTS", "Return from subroutine, Implied", &cpu_RTS, &cpu_IMP, 6 },	        {"ADC", "Add with Carry, X-indexed, Indirect", &cpu_ADC, &cpu_IZX, 6 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 3 },	                {"ADC", "Add with Carry, Zero Paged", &cpu_ADC, &cpu_ZP0, 3 },	            {"ROR", "Rotate Right, Zero Paged", &cpu_ROR, &cpu_ZP0, 5 },	                        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },	    {"PLA", "Pull A from Stack, Implied", &cpu_PLA, &cpu_IMP, 4 },	    {"ADC", "Add with Carry, Immediate", &cpu_ADC, &cpu_IMM, 2 },	                {"ROR", "Rotate Right, Implied", &cpu_ROR, &cpu_IMP, 2 },	        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	{"JMP", "Jump, Indirect", &cpu_JMP, &cpu_IND, 5 },	            {"ADC", "Add with Carry, Absolute", &cpu_ADC, &cpu_ABS, 4 },	                    {"ROR", "Rotate Right, Absolute", &cpu_ROR, &cpu_ABS, 6 },	                    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },
		{"BVS", "Branch on Overflow Set, Relative", &cpu_BVS, &cpu_REL, 2 },	        {"ADC", "Add with Carry, Y-indexed, Indirect", &cpu_ADC, &cpu_IZY, 5 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },    	{"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	                {"ADC", "Add with Carry, Zero Paged, X-indexed", &cpu_ADC, &cpu_ZPX, 4 },	{"ROR", "Rotate Right, Zero Paged, X-indexed", &cpu_ROR, &cpu_ZPX, 6 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },	    {"SEI", "Set Interrupt Disable, Implied", &cpu_SEI, &cpu_IMP, 2 },	{"ADC", "Add with Carry, Absolute, Y-indexed", &cpu_ADC, &cpu_ABY, 4 },	        {"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 2 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	            {"ADC", "Add with Carry, Absolute, X-indexed", &cpu_ADC, &cpu_ABX, 4 },	        {"ROR", "Rotate Right, Absolute, X-indexed", &cpu_ROR, &cpu_ABX, 7 },	        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },
		{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 2 },	                        {"STA", "Store A, X-indexed, Indirect", &cpu_STA, &cpu_IZX, 6 },	            {"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },	{"STY", "Store Y, Zero Paged", &cpu_STY, &cpu_ZP0, 3 },              {"STA", "Store A, Zero Paged", &cpu_STA, &cpu_ZP0, 3 },	                    {"STX", "Store X, Zero Page", &cpu_STX, &cpu_ZP0, 3 },	                            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 3 },	    {"DEY", "Decrement Y, Implied", &cpu_DEY, &cpu_IMP, 2 },	            {"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 2 },	                            {"TXA", "Transfer X to A, Implied", &cpu_TXA, &cpu_IMP, 2 },	        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	{"STY", "Store Y, Absolute", &cpu_STY, &cpu_ABS, 4 },	        {"STA", "Store A, Absolute", &cpu_STA, &cpu_ABS, 4 },	                        {"STX", "Store X, Absolute", &cpu_STX, &cpu_ABS, 4 },	                        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 4 },
		{"BCC", "Branch on Carry Clear, Relative", &cpu_BCC, &cpu_REL, 2 },	        {"STA", "Store A, Y-indexed, Indirect", &cpu_STA, &cpu_IZY, 6 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },	{"STY", "Store Y, Zero Page, X-indexed", &cpu_STY, &cpu_ZPX, 4 },	{"STA", "Store A, Zero Paged, X-indexed", &cpu_STA, &cpu_ZPX, 4 },	        {"STX", "Store X, Zero Page, Y-indexed", &cpu_STX, &cpu_ZPY, 4 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 4 },	    {"TYA", "Transfer Y to A, Implied", &cpu_TYA, &cpu_IMP, 2 },	        {"STA", "Store A, Absolute, Y-indexed", &cpu_STA, &cpu_ABY, 5 },	                {"TXS", "Transfer X to SP, Implied", &cpu_TXS, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 5 },	            {"STA", "Store A, Absolute, X-indexed", &cpu_STA, &cpu_ABX, 5 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },	                            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },
		{"LDY", "Load Y, Immediate", &cpu_LDY, &cpu_IMM, 2 },	                    {"LDA", "Load A, X-indexed, Indirect", &cpu_LDA, &cpu_IZX, 6 },	            {"LDX", "Load X, Immediate", &cpu_LDX, &cpu_IMM, 2 },	{"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },	{"LDY", "Load Y, Zero Paged", &cpu_LDY, &cpu_ZP0, 3 },	            {"LDA", "Load A Register", &cpu_LDA, &cpu_ZP0, 3 },	                        {"LDX", "Load X, Zero Paged", &cpu_LDX, &cpu_ZP0, 3 },	                            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 3 },	    {"TAY", "Transfer A to Y, Implied", &cpu_TAY, &cpu_IMP, 2 },	        {"LDA",  "Load A Register, Immediate", &cpu_LDA, &cpu_IMM, 2 },	                {"TAX", "Transfer A to X, Implied", &cpu_TAX, &cpu_IMP, 2 },	        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	{"LDY", "Load Y, Absolute", &cpu_LDY, &cpu_ABS, 4 },	            {"LDA",  "Load A Register", &cpu_LDA, &cpu_ABS, 4 },	                            {"LDX", "Load X, Absolute", &cpu_LDX, &cpu_ABS, 4 },	                            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 4 },
		{"BCS", "Branch on Carry Set, Relative", &cpu_BCS, &cpu_REL, 2 },	        {"LDA", "Load A, Y-indexed, Indirect", &cpu_LDA, &cpu_IZY, 5 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },	{"LDY", "Load Y, Zero Paged, X-indexed", &cpu_LDY, &cpu_ZPX, 4 },	{"LDA", "Load A Register", &cpu_LDA, &cpu_ZPX, 4 },	                        {"LDX", "Load X, Zero Paged, Y-indexed", &cpu_LDX, &cpu_ZPY, 4 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 4 },	    {"CLV", "Clear Overflow, Implied", &cpu_CLV, &cpu_IMP, 2 },	        {"LDA",  "Load A Register, Absolute, Y-indexed", &cpu_LDA, &cpu_ABY, 4 },	    {"TSX", "Tranfer SP to X, Implied", &cpu_TSX, &cpu_IMP, 2 },	        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 4 },	{"LDY", "Load Y, Absolute, X-indexed", &cpu_LDY, &cpu_ABX, 4 },	{"LDA",  "Load A Register", &cpu_LDA, &cpu_ABX, 4 },	                            {"LDX", "Load X, Absolute, Y-indexed", &cpu_LDX, &cpu_ABY, 4 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 4 },
		{"CPY", "Compare with Y, Immediate", &cpu_CPY, &cpu_IMM, 2 },	            {"CMP", "Compare with A, X-indexed, Indirect", &cpu_CMP, &cpu_IZX, 6 },	    {"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"CPY", "Compare with Y, Zero Paged", &cpu_CPY, &cpu_ZP0, 3 },	    {"CMP", "Compare with A, Zero Paged", &cpu_CMP, &cpu_ZP0, 3 },	            {"DEC", "Decrement, Zero Paged", &cpu_DEC, &cpu_ZP0, 5 },	                        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },	    {"INY", "Increment Y, Implied", &cpu_INY, &cpu_IMP, 2 },	            {"CMP", "Compare with A, Immediate", &cpu_CMP, &cpu_IMM, 2 },	                {"DEX", "Decrement X, Implied", &cpu_DEX, &cpu_IMP, 2 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	{"CPY", "Compare with Y, Absolute", &cpu_CPY, &cpu_ABS, 4 },	    {"CMP", "Compare with A, Absolute", &cpu_CMP, &cpu_ABS, 4 },	                    {"DEC", "Decrement, Absolute", &cpu_DEC, &cpu_ABS, 6 },	                        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },
		{"BNE", "Branch on Not Equal, Relative", &cpu_BNE, &cpu_REL, 2 },	        {"CMP", "Compare with A, Y-indexed, Indirect", &cpu_CMP, &cpu_IZY, 5 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	                {"CMP", "Compare with A", &cpu_CMP, &cpu_ZPX, 4 },	                        {"DEC", "Decrement, Zero Paged, X-indexed", &cpu_DEC, &cpu_ZPX, 6 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },	    {"CLD", "Clear Decimal, Implied", &cpu_CLD, &cpu_IMP, 2 },	        {"CMP", "Comparew with A, Absolute, Y-indexed", &cpu_CMP, &cpu_ABY, 4 },	        {"NOP", "No Operation, Implied", &cpu_NOP, &cpu_IMP, 2 },	        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	            {"CMP", "Compare with A, Absolute, X-indexed", &cpu_CMP, &cpu_ABX, 4 },	        {"DEC", "Decrement, Absolute, X-indexed", &cpu_DEC, &cpu_ABX, 7 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },
		{"CPX", "Compare with X, Immediate", &cpu_CPX, &cpu_IMM, 2 },	            {"SBC", "Subtract with Carry, X-indexed, Indirect", &cpu_SBC, &cpu_IZX, 6 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"CPX", "Compare with X, Zero Paged", &cpu_CPX, &cpu_ZP0, 3 },	    {"SBC", "Subtract with Carry, Zero Paged", &cpu_SBC, &cpu_ZP0, 3 },	        {"INC", "Increment, Zero Paged", &cpu_INC, &cpu_ZP0, 5 },	                        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 5 },	    {"INX", "Increment X, Implied", &cpu_INX, &cpu_IMP, 2 },	            {"SBC", "Subtract with Carry, Immediate", &cpu_SBC, &cpu_IMM, 2 },	            {"NOP", "No Operation, Implied", &cpu_NOP, &cpu_IMP, 2 },	        {"???", "Illegal, Implied", &cpu_SBC, &cpu_IMP, 2 },	{"CPX", "Compare with X, Absolute", &cpu_CPX, &cpu_ABS, 4 },     {"SBC", "Subtract with Carry, Absolute", &cpu_SBC, &cpu_ABS, 4 },	            {"INC", "Increment, Absolute", &cpu_INC, &cpu_ABS, 6 },	                        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },
		{"BEQ", "Branch on Equal, Relative", &cpu_BEQ, &cpu_REL, 2 },            	{"SBC", "Subtract with Carry, Y-indexed, Indirect", &cpu_SBC, &cpu_IZY, 5 },	{"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 2 },	    {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 8 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	                {"SBC", "Subtract with Carry", &cpu_SBC, &cpu_ZPX, 4 },	                    {"INC", "Increment, Zero Paged, X-indexed", &cpu_INC, &cpu_ZPX, 6 },	                {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 6 },	    {"SED", "Set Decimal, Implied", &cpu_SED, &cpu_IMP, 2 },	            {"SBC", "Subtract with Carry, Absolute, Y-indexed", &cpu_SBC, &cpu_ABY, 4 },	    {"NOP", "No Operation, Implied", &cpu_NOP, &cpu_IMP, 2 },	        {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 },	{"???", "Illegal, Implied", &cpu_NOP, &cpu_IMP, 4 },	            {"SBC", "Subtract with Carry, Absolute, X-indexed", &cpu_SBC, &cpu_ABX, 4 },	    {"INC", "Increment, Absolute, X-indexed", &cpu_INC, &cpu_ABX, 7 },	            {"???", "Illegal, Implied", &cpu_XXX, &cpu_IMP, 7 }
	};
	
///////////////////////////////////////////////////////////////////////////////
// Disassembler FUNCTIONS
	
	/**
	* Write formatted hash string to console
	* @return void
	*/
		void getHashString(void *hex, size_t size){
			
			if(size == 2){
			
				uint8_t num = *(uint8_t*) hex;
				char hash[5];
				sprintf(hash,"%02X", num);
				printf("%s", hash);
			
			} else {
			
				uint16_t num = *(uint16_t*) hex;
				char hash[] = "0000";
				sprintf(hash,"%04X", num);
				hash[size] = '\0';
				printf("%s", hash);
			
			}
		
		}
		
	/**
	* Pust hex string in dest.
	* @return void
	*/
		void hex(uint16_t hex, size_t size, char * dest) {
			size = (size > 8) ? 8 : size;
			char tmp[9]; //max 8 chars
			snprintf(tmp, 8,"%04X", hex);
			tmp[size] = '\0';
			strncpy(dest, tmp, size+1);
		}
		
	/**
	* Fill code[] with NOP instructions, ready for disassembler to fill in
	* @return void
	*/
		void code_fillBlank(){
	
			char hashTemp[5];
	
			for(int i = 0; i <= MAX_MEM; i++){
				char line[50] = "0x";
				hex(i,4, hashTemp);
				strcat(line, hashTemp);
				strcat(line, ": NOP\t{IMP}");
				strcpy(code[i], line);
			}
		
		}
		
	/**
	* Draw out surrounding instructions from code[] at given memory location
	* @return void
	*/
		void code_draw(uint16_t addr, uint8_t linesAmount){
		
			char lines[linesAmount][50];
			
			int lower = 0, count = 0, nullLines = 0;
			uint16_t start;
			uint16_t lastOpAddrFound = 0;
			start = addr;
			
			//adjust so always oddd
				if(linesAmount % 2 != 0){
					linesAmount++;
				}
			
			//midpoint
				uint8_t midPoint = (uint8_t)(linesAmount / 2)-1;
			
			//go backwards finding the 3 instructions before the current.
			//in the dissassembler, inter-instruction memorylocations are padded with ---
			//these are ignored as aren't instructions
			//programs are always surrounded with NOP commands from memory + code[] init
				while(lower < midPoint){
					
					start--;
					
					if(strcmp(code[start], "---") != 0){
						lower++;
						lastOpAddrFound = start;
					} else {
					
						nullLines++;
						if(nullLines == 6){
							start = lastOpAddrFound;
							
							for(int i = 0; i <= midPoint-lower; i++){
								strcpy(lines[i], "---");
							}
							
							count = midPoint-lower;
							lower = midPoint;
						}
						
					}
				
				}
				
			//now move forward from this position, ignore --- and store the op line
				while(count < linesAmount){
				
					if(strcmp(code[start], "---") != 0){
						strcpy(lines[count], code[start]);
						count++;
					}
					
					start++;
				
				}
			
			//finally now we have full lines, output them highlighting current instruction
				for(int i = 0; i < linesAmount; i++){
					if(i == midPoint){
						printf("%s%s%s\n", ANSI_COLOR_CYAN, lines[i], ANSI_COLOR_RESET);
					} else {
						printf("%s\n", lines[i]);
					}
				}
		
		}
	
///////////////////////////////////////////////////////////////////////////////
// BUS FUNCTIONS
	
	/**
	* Add "deives" to bus struct. May be used in future to change bus attached devices
	* @return void
	*/
		void bus_add_devices()
		{
		
			printf("%sAdded Display device to bus.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
			bus.dis = dis;
		
			printf("%sAdded Ram device to bus.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
			bus.ram = ram;
		
		}
	
	/**
	* Write to attached RAM through Bus
	* @return void
	*/
		void bus_write(uint16_t addr, uint8_t data, bool output)
		{
		
			bus.ram.data[addr] = data;
			
			//we're writing to display memory location, so update display
				if(addr >= displayMemLoc && addr <= displayMemLoc+displayMemLen && output){
					printf("--> Wrote 0X");
					getHashString(&data, 2);
					printf(" [%d] into display memory: 0X", addr);
					getHashString(&addr, 4);
					printf("\n");
				}
		
		}
	
	/**
	* Read RAM through Bus
	* @return uint8_t
	*/
		uint8_t bus_read(uint16_t addr)
		{
			
			return bus.ram.data[addr];
			
		}
	
///////////////////////////////////////////////////////////////////////////////
// Display FUNCTIONS

	/**
	* Read from a memory location
	* @return uint8_t
	*/
		uint8_t dis_read(uint16_t addr){
			return bus_read(addr);
		}
		
	/**
	* Write to a memory location if within range
	* @return uint8_t
	*/
		uint8_t dis_write(uint16_t addr, uint8_t data, bool output){
			if(addr >= displayMemLoc && addr <= displayMemLoc+displayMemLen){
				bus_write(addr, data, output);
			}
		}

	/**
	* Clear the console with blank chars (0x20 [32]), then reset console count
	* @return void
	*/
		void dis_console_clear(){
		
			for(uint16_t i = displayMemLoc+disConsoleOffset; i < displayMemLoc+displayMemLen; i++){
				bus.ram.data[i] = ' ';
			}
			
			dis.ccount = 0;
		
		}
		
	/**
	* Clear the buffer with blank chars (0x20 [32])
	* @return void
	*/
		void dis_buffer_clear(){

			for(uint16_t i = 0; i < 32; i++){
				dis_write(displayMemLoc+disBufferOffset+i, ' ', false);
			}
			
			dis.bCount = 0;
			
		}
		
	/**
	* Clear the display op memory location
	* @return void
	*/
		void dis_clear_op(){
			bus.ram.data[displayMemLoc] = 0x0000;
		}
		
	/**
	* Reset all on the display
	* @return void
	*/
		void dis_reset(){
			
			//program counter basically always looks at this mem addr
				dis.PC = displayMemLoc;
			
			//make sure first instruction is always No Update
				dis_clear_op();
			
			//clear buffer
				dis_buffer_clear();
			
			//clear console
				dis_console_clear();
			
		}
		
	/**
	* Parse an opcode and fire off it's function using the display lookup array
	* @return void
	*/
		void dis_execute(){
		
			printf("%s---------------------------------------------%s\n\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
				
			// Read next instruction byte. This 8-bit value is used to index
			// the translation table to get the relevant information about
			// how to implement the instruction
				disOPCode = dis_read(dis.PC);
				
			//get opcode struct and display info about this operation
				printf("%s--- Display Instruction Opcode: %#X [%s] (%s)%s\n", ANSI_COLOR_YELLOW,disOPCode, lookupDis[disOPCode].name, lookupDis[disOPCode].label,  ANSI_COLOR_RESET);
	
			// Get Starting number of cycles from instruction struct
				
			// Perform operation, calling the function defined in that element of the instruction struct array
				(*lookupDis[disOPCode].operate)();
		
		}
		
	/**
	* Display Function: No Operation
	* @return uint8_t
	*/
		uint8_t dis_NOP(){
			dis_clear_op();
		}
		
	/**
	* Display Function: Add Character to Buffer
	* @return uint8_t
	*/
		uint8_t dis_ACB(){
			uint8_t value = dis_read(dis.PC+1);
			if(dis.bCount < 32){
				dis_write(displayMemLoc+disBufferOffset+dis.bCount, value, true);
				dis.bCount++;
			}
			dis_clear_op();
			return 0;
		}
		
	/**
	* Display Function: Convert literal number to characters and add to Buffer
	* @return uint8_t
	*/
		uint8_t dis_ANB(){
		
			int num = dis_read(dis.PC+1);
			num = '0' + num;
			if(dis.bCount < 32){
				dis_write(displayMemLoc+disBufferOffset+dis.bCount, (uint8_t)num, true);
				dis.bCount++;
			}
			dis_clear_op();
			return 0;
		}
		
	/**
	* Display Function: Write the contents of the buffer to the next line of the console,
	* shifting the console contents if already full
	* @return uint8_t
	*/
		uint8_t dis_WRB(){
		
			//shift console arrays down
				if(dis.ccount > 7){
				
					char lines[7][32];
					int character = 0;
					
					uint16_t memStart = displayMemLoc+disConsoleOffset+32;  //0x9050 ignore first 32 as that will be overwritten.
					uint16_t memUpper = displayMemLoc+displayMemLen;        //0x9130
					
					for(uint16_t i = memStart; i < memUpper; i++){
						
						//start from line two in console as line 1 is being discarded
						if(i >= memStart && i < memStart+32){
						
							lines[0][character] = dis_read(i);
						
						} else if(i >= memStart+32 && i < memStart+64){
						
							lines[1][character] = dis_read(i);
						
						} else if(i >= memStart+64 && i < memStart+96){
						
							lines[2][character] = dis_read(i);
						
						} else if(i >= memStart+96 && i < memStart+128){
						
							lines[3][character] = dis_read(i);
						
						} else if(i >= memStart+128 && i < memStart+160){
						
							lines[4][character] = dis_read(i);
						
						} else if(i >= memStart+160 && i < memStart+192){
						
							lines[5][character] = dis_read(i);
						
						} else if(i >= memStart+192){
						
							lines[6][character] = dis_read(i);
						
						}
						
						character++;
						
						if(character % 32 == 0 && i > memStart){
							character = 0;
						}
						
					}
					
					//now go through lines array, starting at start of console memory.
						memStart = displayMemLoc+disConsoleOffset; //reset memStart to start of console now
						uint8_t line = 0;
						character = 0;
						for(uint16_t i = memStart; i < memUpper; i++){

							if(character == 0 && i > memStart){
								line++;
							}

							if(line == 7){

								dis_write(i, ' ', false);

							} else {

								dis_write(i, lines[line][character], false);

							}
							
							character++;
						
							if(character % 32 == 0 && i > memStart){
								character = 0;
							}

						}
						
					//set buffer to add to last console line location
						dis.ccount = 7;
				
				}
				
			//now write up to 32 buffer bytes to last line location in console memory
				for(uint8_t i = 0; i < 32; i++){
				
					dis_write((displayMemLoc+disConsoleOffset+(32*dis.ccount)+i), bus.ram.data[displayMemLoc+disBufferOffset+i], true);
				
				}
				
			dis_clear_op();
			dis_buffer_clear();
			dis.ccount++;
			return 0;
		
		}
		
	/**
	* Display Function: Clear the console
	* @return uint8_t
	*/
		uint8_t dis_CLC(){
			dis_clear_op();
			return 0;
		}
		
	/**
	* Display Function: Clear the buffer
	* @return uint8_t
	*/
		uint8_t dis_CLB(){
			dis_clear_op();
			return 0;
		}
	
	/**
	* Draw the whole console
	* @return void
	*/
		void dis_draw(){
			
			//10 dash's
				printf("\n");
			
			//lines
				uint16_t addr = displayMemLoc+disConsoleOffset;
				for(uint16_t i = 0; i < 8; i++){
				
					printf("| >");
					
					for(uint16_t j = 0; j < 32; j++){
					
						printf("%c", bus.ram.data[addr]);
						addr++;
					
					}
					
					printf("\n");
				
				}
			
			printf("\n");
			
		}

///////////////////////////////////////////////////////////////////////////////
// RAM FUNCTIONS

	/**
	* Clear RAM by initialising to NOP instructions
	* @return void
	*/
		void ram_clear(){
		
			printf("%sCleared Ram with NOP's.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
		
			int i;
			for(i = 0; i < MAX_MEM; i++){
			
				bus.ram.data[i] = 0xEA;
			
			}
		
		}
		
	/**
	* Load hash string into memory and set reset vector to start of program
	* @return void
	*/
		void ram_load_program(){
		
			printf("%sRam program mounted.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
			
			FILE * fp;
			
			fp = fopen("post.bin", "rb");
			
			programStart = 0x8000;
			uint16_t nOffset = programStart;
			
			if(fp != NULL){
			
				//find out file size to make program array that size.
					fseek(fp, 0L, SEEK_END);
					long sz = ftell(fp);
				
				//set cursor back to start of file
					fseek(fp, 0, SEEK_SET);
				
				//set prog as same size as file in bytes
					uint8_t prog[sz];
				
				//go through file byte by byte assigning to program array
					uint8_t byteHere;
					for(long i = 0; i < sz; i++){
					
						byteHere = fgetc(fp);
					
						//check if end of file now
							if(feof(fp)){
								break;
							}
						
						prog[i] = byteHere;
					
					}
				
				//write that to memory
					programSize = sz;
					int i = 0;
					while (i < programSize)
					{
						bus.ram.data[nOffset] = prog[i];
						nOffset++;
						i++;
					}
			
			} else {
		
				// Load Program (assembled at https://www.masswerk.at/6502/assembler.html)
				// calculate 3 X 10
				/*
					*=$8000
					LDX #10
					STX $0000
					LDX #3
					STX $0001
					LDY $0000
					LDA #0
					CLC
					loop
					ADC $0001
					DEY
					BNE loop
					STA $0002
					NOP
					NOP
					NOP
				*/
				
				// Convert hex string into bytes for RAM
				//A2 0A 8E 00 00 A2 03 8E 01 00 AC 00 00 A9 00 18 6D 01 00 88 D0 FA 8D 02 00 EA EA EA
				//uint8_t prog[] = {0xA2,0x0A,0x8E,0x00,0x00,0xA2,0x03,0x8E,0x01,0x00,0xAC,0x00,0x00,0xA9,0x00,0x18,0x6D,0x01,0x00,0x88,0xD0,0xFA,0x8D,0x02,0x00,0x8D,0x00,0x90,0xEA,0xEA,0XEA,0XEA };
				//uint8_t prog[] = {0xA2,0x0A,0x8E,0x00,0x00,0xA2,0x03,0x8E,0x01,0x00,0xAC,0x00,0x00,0xA9,0x00,0x18,0x6D,0x01,0x00,0x88,0xD0,0xFA,0x8D,0x02,0x00,0xA2,0x00,0x8E,0x10,0x90,0x8D,0x11,0x90};
				//uint8_t prog[] = {0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90};
				
				/*
				*
					LDA #$48 ;H
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$65 ;e
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$6C ;l
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$6C ;l
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$6F ;o
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$20 ;space
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$57 ;W
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$6F ;o
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$72 ;r
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$6C ;l
					STA $9001
					LDX #$1
					STX $9000
					
					LDA #$64 ;l
					STA $9001
					LDX #$1
					STX $9000
					
					LDX #$3
					STX $9000
				*/
				
				uint8_t prog[] = {
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x64,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x64,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x64,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x64,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x64,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x64,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x64,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90,
					0xA9,0x48,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x20,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x57,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6F,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x72,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x6C,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA9,0x65,0x8D,0x01,0x90,0xA2,0x01,0x8E,0x00,0x90,0xA2,0x03,0x8E,0x00,0x90
				};
				
				programSize = sizeof prog / sizeof prog[0];
				int i = 0;
				while (i < programSize)
				{
					bus.ram.data[nOffset] = prog[i];
					nOffset++;
					i++;
				}
				
			}
	
			// Set Reset Vector to start of program
				bus.ram.data[0xFFFC] = 0x00;
				bus.ram.data[0xFFFD] = 0x80;
		
		}
		
	/**
	* Draw the contents of memory in a matrix of set height and width
	* @return void
	*/
		void ram_draw(uint16_t nAddr, int nRows, int nColumns)
		{
			
			printf("%sMemLoc\t", ANSI_COLOR_CYAN);
			
			for (int col = 0; col < nColumns; col++){
				if(col == 8){
					printf("   ");
				}
				printf("  %01X",  col);
			}
			
			printf("%s\n",  ANSI_COLOR_RESET);
			
			for (int row = 0; row < nRows; row++){
			
				printf("%s0x", ANSI_COLOR_MAGENTA);
				getHashString(&nAddr, 3);
				printf("x\t%s", ANSI_COLOR_RESET);
				
				for (int col = 0; col < nColumns; col++){
				
					if(col == 8){
						printf("   ");
					}
				
					if(bus.ram.data[nAddr] == 0){
						printf(" 00");
					} else {
						printf(" %02X",  bus.ram.data[nAddr]);
					}
					nAddr += 1;
				}
				
				printf("\t|");
				
				nAddr -= nColumns;
				for (int col = 0; col < nColumns; col++){
					
					if(col == 8){
						printf("| |");
					}
				
					if ((unsigned char)bus.ram.data[nAddr] >= ' ' && (unsigned char)bus.ram.data[nAddr] <= '~'){
						printf("%c",  bus.ram.data[nAddr]);
					} else {
						printf(".");
					}
					
					nAddr += 1;
				}
				
				printf("|\n");
				
			}
			
			printf("%s", ANSI_COLOR_RESET);
			
		}
	
///////////////////////////////////////////////////////////////////////////////
// CPU FUNCTIONS

	/**
	* Draw current CPU data status
	* @return void
	*/
		void cpu_draw()
		{
			printf("            STATUS:");
			printf(" %sN%s", (cpu.status & N ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
			printf(" %sV%s", (cpu.status & V ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
			printf(" %s-%s", (cpu.status & U ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
			printf(" %sB%s", (cpu.status & B ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
			printf(" %sD%s", (cpu.status & D ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
			printf(" %sI%s", (cpu.status & I ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
			printf(" %sZ%s", (cpu.status & Z ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
			printf(" %sC%s", (cpu.status & C ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
			printf(
				"\n%sPC: %#X %sSP: %#X %sA: %#X [%d] %sX: %#X [%d] %sY: %#X [%d] %s",
				ANSI_COLOR_GREEN,
				cpu.PC,
				ANSI_COLOR_MAGENTA,
				cpu.SP,
				ANSI_COLOR_CYAN,
				cpu.A,
				cpu.A,
				ANSI_COLOR_YELLOW,
				cpu.X,
				cpu.X,
				ANSI_COLOR_BLUE,
				cpu.Y,
				cpu.Y,
				ANSI_COLOR_RESET);
		}
		
	/**
	* Returns the char that represents a given bit field flag
	* @return char
	 */
		char flagStr(FLAGS6502 f){
			if(f == C)
				return 'C';
			else if (f == Z)
				return 'Z';
			else if (f == I)
				return 'I';
			else if (f == D)
				return 'D';
			else if (f == B)
				return 'B';
			else if (f == U)
				return 'U';
			else if (f == V)
				return 'V';
			else if (f == N)
				return 'N';
		}
	
	/**
	* Returns the value of a specific bit of the cpu.status register
	* @return uint8_t
	 */
		uint8_t cpu_get_flag(FLAGS6502 f)
		{
			printf("%s<:: Flag %c got%s\n",ANSI_COLOR_BLUE, flagStr(f), ANSI_COLOR_RESET);
			return ((cpu.status & f) > 0) ? 1 : 0;
		}
	
	/**
	* Sets or clears a specific bit of the cpu.status register
	* @return uint8_t
	 */
		void cpu_set_flag(FLAGS6502 f, bool v)
		{
		
			char flag = flagStr(f);
			char onOff = (v) ? '1' : '0';
			
			if(v){
				printf("%s::> Flag %c set to %c%s\n",ANSI_COLOR_GREEN, flag, onOff, ANSI_COLOR_RESET);
			} else {
				printf("%s::> Flag %c set to %c%s\n",ANSI_COLOR_RED, flag, onOff, ANSI_COLOR_RESET);
			}
		
			if (v){
				cpu.status |= f;
			} else {
				cpu.status &= ~f;
			}
		
		}
		
	/**
	* Set Accumulator
	* @param uint8_t data
	* @return void
	*/
		void cpu_set_a(uint8_t data){
			printf("%s~~> A  set to\t0X", ANSI_COLOR_CYAN);
			getHashString(&data, 2);
			printf("\t[%d]%s\n", data, ANSI_COLOR_RESET);
			cpu.A = data;
		}
	
	/**
	* Set X Register
	* @param uint8_t data
	* @return void
	*/
		void cpu_set_x(uint8_t data){
			printf("%s~~> X  set to\t0X", ANSI_COLOR_YELLOW);
			getHashString(&data, 2);
			printf("\t[%d]%s\n", data, ANSI_COLOR_RESET);
			cpu.X = data;
		}
	
	/**
	* Set Y Register
	* @param uint8_t data
	* @return void
	*/
		void cpu_set_y(uint8_t data){
			printf("%s~~> X  set to\t0X", ANSI_COLOR_BLUE);
			getHashString(&data, 2);
			printf("\t[%d]%s\n", data, ANSI_COLOR_RESET);
			cpu.Y = data;
		}
	
	/**
	* Set Program Counter
	* @param uint16_t data
	* @return void
	*/
		void cpu_set_pc(uint16_t data){
			printf("%s==> PC set to\t0X", ANSI_COLOR_GREEN);
			getHashString(&data, 4);
			printf("\t[%d]%s\n", data, ANSI_COLOR_RESET);
			cpu.PC = data;
		}
	
	/**
	* Set Stack Pointer
	* @param uint8_t data
	* @return void
	*/
		void cpu_set_sp(uint8_t data){
			printf("%s++> SP set to\t0X", ANSI_COLOR_MAGENTA);
			getHashString(&data, 2);
			printf("\t[%d]%s\n", data, ANSI_COLOR_RESET);
			cpu.SP = data;
		}
	
	/**
	* Read RAM through Cpu->Bus
	* @param uint16_t addr
	* @return uint8_t
	*/
		uint8_t cpu_read(uint16_t addr)
		{
			uint8_t data = bus_read(addr);
			printf("<-- CPU Read 0X");
			getHashString(&data, 2);
			printf(" [%d] from memory: 0X", data);
			getHashString(&addr, 4);
			printf("\n");
			return data;
		}
	
	/**
	* Write to RAM through Cpu->Bus
	* @param uint16_t addr
	* @param uint8_t data
	* @return void
	*/
		void cpu_write(uint16_t addr, uint8_t data)
		{
			bus_write(addr, data, true);
			printf("--> Wrote 0X");
			getHashString(&data, 2);
			printf(" [%d] into memory: 0X", data);
			getHashString(&addr, 4);
			printf("\n");
		}
		
	/**
	* This function sources the data used by the instruction into
	* a convenient numeric variable. Some instructions dont have to
	* fetch data as the source is implied by the instruction. For example
	* "INX" increments the X register. There is no additional data
	* required. For all other addressing modes, the data resides at
	* the location held within cpuAddrAbs, so it is read from there.
	* Immediate adress mode exploits this slightly, as that has
	* set cpuAddrAbs = cpu.PC + 1, so it fetches the data from the
	* next byte for example "LDA $FF" just loads the accumulator with
	* 256, i.e. no far reaching memory fetch is required. "cpuFetched"
	* is a variable global to the CPU, and is set by calling this
	* function. It also returns it for convenience.
	* @return uint8_t
	 */
		uint8_t cpu_fetch(){
			if (lookupCpu[cpuOPCode].addrmode != &cpu_IMP){
				cpuFetched = cpu_read(cpuAddrAbs);
			}
			return cpuFetched;
		}
		
	/**
	* Is the CPU executing
	* @return bool
	*/
		bool cpu_running(){
			return cpuExecuting;
		}
	
	/**
	* Run through cycles set for each instruction
	* @return void
	*/
		void cpu_run_cycles()
		{
			
			cpuClockCount += cpuCycles;
			
			if(cuCyclesInit){
				printf("%s### %d Total Cycles, Instruction Executed in %d Cycles ", ANSI_COLOR_MAGENTA,cpuClockCount, cpuCycles);
				cuCyclesInit = false;
			}
			
			while(cpuCycles > 0){
				printf("%s%d ", ANSI_COLOR_BLUE, cpuCycles);
				cpuCycles--;
			}
			
			printf("\n%s", ANSI_COLOR_RESET);
		
		}
	 
	/**
	* Forces the 6502 into a known state. This is hard-wired inside the CPU. The
	* registers are set to 0x00, the status register is cleared except for unused
	* bit which remains at 1. An absolute address is read from location 0xFFFC
	* which contains a second address that the program counter is set to. This
	* allows the programmer to jump to a known and programmable location in the
	* memory to start executing from. Typically the programmer would set the value
	* at location 0xFFFC at compile time.
	* @return void
	*/
		void cpu_reset(){
		
			printf("\n%s---------------------------------------------%s\n\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
			printf("%s--- Reset CPU PC to reset vector.%s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);
		
			// Get address to set program counter to
				cpuAddrAbs = 0xFFFC;
				uint16_t lo = cpu_read(cpuAddrAbs);
				cpuAddrAbs = 0xFFFD;
				uint16_t hi = cpu_read(cpuAddrAbs);
		
			// Set it
				cpu_set_pc((hi << 8) | lo);
				
			//so code_drawstarts at correct place
				cpuLastOpAddr = cpu.PC;
		
			// Reset internal registers
				cpu_set_a(0);
				cpu_set_x(0);
				cpu_set_y(0);
				cpu_set_sp(0xFD);
				
				cpu.status = 0x00 | U;
				printf("%s--- Status Flag Cleared.%s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);
		
			// Clear internal helper variables
				cpuAddrRel = 0x0000;
				cpuAddrAbs = 0x0000;
				cpuFetched = 0x00;
		
			// Reset takes time
				cpuCycles = 8;
				cuCyclesInit = true;
				cpu_run_cycles();
			
		}
	
	/**
	* Interrupt requests are a complex operation and only happen if the
	* "disable interrupt" flag is 0. IRQs can happen at any time, but
	* you dont want them to be destructive to the operation of the running
	* program. Therefore the current instruction is allowed to finish
	* (which I facilitate by doing the whole thing when cycles == 0) and
	* then the current program counter is stored on the stack. Then the
	* current status register is stored on the stack. When the routine
	* that services the interrupt has finished, the status register
	* and program counter can be restored to how they where before it
	* occurred. This is impemented by the "RTI" instruction. Once the IRQ
	* has happened, in a similar way to a reset, a programmable address
	* is read form hard coded location 0xFFFE, which is subsequently
	* set to the program counter.
	* @return void
	* */
		void cpu_irq(){
			// If interrupts are allowed
				if (cpu_get_flag(I) == 0)
				{
					// Push the program counter to the stack. It's 16-bits dont
					// forget so that takes two pushes
						cpu_write(0x0100 + cpu.SP, (cpu.PC >> 8) & 0x00FF);
						cpu.SP--;
						cpu_write(0x0100 + cpu.SP, cpu.PC & 0x00FF);
						cpu.SP--;
			
					// Then Push the status register to the stack
						cpu_set_flag(B, 0);
						cpu_set_flag(U, 1);
						cpu_set_flag(I, 1);
						cpu_write(0x0100 + cpu.SP, cpu.status);
						cpu.SP--;
			
					// Read new program counter location from fixed address
						cpuAddrAbs = 0xFFFE;
						uint16_t lo = cpu_read(cpuAddrAbs + 0);
						uint16_t hi = cpu_read(cpuAddrAbs + 1);
						cpu_set_pc((hi << 8) | lo);
			
					// IRQs take time
						cpuCycles = 7;
						cuCyclesInit = true;
				}
		}
	
	/**
	* A Non-Maskable Interrupt cannot be ignored. It behaves in exactly the
	* same way as a regular IRQ, but reads the new program counter address
	* form location 0xFFFA.
	* @return void
	*/
		void cpu_nmi(){
			cpu_write(0x0100 + cpu.SP, (cpu.PC >> 8) & 0x00FF);
			cpu.SP--;
			cpu_write(0x0100 + cpu.SP, cpu.PC & 0x00FF);
			cpu.SP--;
		
			cpu_set_flag(B, 0);
			cpu_set_flag(U, 1);
			cpu_set_flag(I, 1);
			cpu_write(0x0100 + cpu.SP, cpu.status);
			cpu.SP--;
		
			cpuAddrAbs = 0xFFFA;
			uint16_t lo = cpu_read(cpuAddrAbs + 0);
			uint16_t hi = cpu_read(cpuAddrAbs + 1);
			cpu_set_pc((hi << 8) | lo);
		
			cpuCycles = 8;
			cuCyclesInit = true;
		}
	
	/**
	* Perform one operations worth of emulation
	* @return void
	*/
		void cpu_execute()
		{
		
			printf("%s---------------------------------------------%s\n\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
		
			// Each instruction requires a variable number of clock cycles to execute.
			// In my emulation, I only care about the final result and so I perform
			// the entire computation in one hit. In hardware, each clock cycle would
			// perform "microcode" style transformations of the CPUs state.
			//
			// To remain compliant with connected devices, it's important that the
			// emulation also takes "time" in order to execute instructions, so I
			// implement that delay by simply counting down the cycles required by
			// the instruction. When it reaches 0, the instruction is complete, and
			// the next one is ready to be executed.
			
			//1 cycle
				//-------------------------------------------------------------------------------
				
					// Read next instruction byte. This 8-bit value is used to index
					// the translation table to get the relevant information about
					// how to implement the instruction
						cpuLastOpAddr = cpu.PC;
						cpuOPCode = cpu_read(cpu.PC);
						
					//get opcode struct and display info about this operation
						printf("%s--- Instruction Opcode: %#X [%s] (%s)%s\n", ANSI_COLOR_YELLOW,cpuOPCode, lookupCpu[cpuOPCode].name, lookupCpu[cpuOPCode].label,  ANSI_COLOR_RESET);
						
					// Increment program counter, we have now read the cpuOPCode byte
						cpu.PC++;
					
				//-------------------------------------------------------------------------------
	
			// Get Starting number of cycles from instruction struct
				cpuCycles = lookupCpu[cpuOPCode].cycles;
				cuCyclesInit = true;
	
			// Perform fetch of intermmediate data using the
			// required addressing mode
				uint8_t additional_cycle1 = (*lookupCpu[cpuOPCode].addrmode)();
	
			//1+ cycles
				//-------------------------------------------------------------------------------
				
					// Perform operation, calling the function defined in that element of the instruction struct array
						uint8_t additional_cycle2 = (*lookupCpu[cpuOPCode].operate)();
						
				//-------------------------------------------------------------------------------
	
			// The addressmode and operation may have altered the number
			// of cycles this instruction requires before its completed
				cpuCycles += (additional_cycle1 & additional_cycle2);
	
			// Always set the unused status flag bit to 1
				//cpu_set_flag(U, true);
				
			//output cycles for this operation
				cpu_run_cycles();
				
			if(cpu.PC == 0xFFFF){
				cpuExecuting = false;
			}
				
		}

///////////////////////////////////////////////////////////////////////////////
// ADDRESSING MODES

	/*
	* The 6502 can address between 0x0000 - 0xFFFF. The high byte is often referred
	* to as the "page", and the low byte is the offset into that page. This implies
	* there are 256 pages, each containing 256 bytes.
	*
	* Several addressing modes have the potential to require an additional clock
	* cycle if they cross a page boundary. This is combined with several instructions
	* that enable this additional clock cycle. So each addressing function returns
	* a flag saying it has potential, as does each instruction. If both instruction
	* and address function return 1, then an additional clock cycle is required.
	* */
	
	/**
	* Address Mode: Implied
	* There is no additional data required for this instruction. The instruction
	* does something very simple like like sets a cpu.status bit. However, we will
	* target the accumulator, for instructions like PHA
	* @return uint8_t
	*/
		uint8_t cpu_IMP(){
			cpuFetched = cpu.A;
			return 0;
		}
	
	/**
	* Address Mode: Immediate
	* The instruction expects the next byte to be used as a value, so we'll prep
	* the read address to point to the next byte
	* @return uint8_t
	*/
		uint8_t cpu_IMM(){
			cpuAddrAbs = cpu.PC++;
			return 0;
		}
	
	
	/**
	* Address Mode: Zero Page
	* To save program bytes, zero page addressing allows you to absolutely address
	* a location in first 0xFF bytes of address range. Clearly this only requires
	* one byte instead of the usual two.
	* @return uint8_t
	*/
		uint8_t cpu_ZP0()
		{
			cpuAddrAbs = cpu_read(cpu.PC);
			cpu.PC++;
			cpuAddrAbs &= 0x00FF;
			return 0;
		}
	
	
	/**
	* Address Mode: Zero Page with X Offset
	* Fundamentally the same as Zero Page addressing, but the contents of the X Register
	* is added to the supplied single byte address. This is useful for iterating through
	* ranges within the first page.
	* @return uint8_t
	*/
		uint8_t cpu_ZPX(){
			cpuAddrAbs = (cpu_read(cpu.PC) + cpu.X);
			cpu.PC++;
			cpuAddrAbs &= 0x00FF;
			return 0;
		}
	
	/**
	* Address Mode: Zero Page with Y Offset
	* Same as above but uses Y Register for offset
	* @return uint8_t
	*/
		uint8_t cpu_ZPY(){
			cpuAddrAbs = (cpu_read(cpu.PC) + cpu.Y);
			cpu.PC++;
			cpuAddrAbs &= 0x00FF;
			return 0;
		}
	
	/**
	* Address Mode: Relative
	* This address mode is exclusive to branch instructions. The address
	* must reside within -128 to +127 of the branch instruction, i.e.
	* you cant directly branch to any address in the addressable range.
	* @return uint8_t
	*/
		uint8_t cpu_REL(){
			cpuAddrRel = cpu_read(cpu.PC);
			cpu.PC++;
			if (cpuAddrRel & 0x80)
				cpuAddrRel |= 0xFF00;
			return 0;
		}
	
	/**
	* Address Mode: Absolute
	* A full 16-bit address is loaded and used from two bytes
	* @return uint8_t
	*/
		uint8_t cpu_ABS(){
		
			uint16_t lo = cpu_read(cpu.PC);
			cpu.PC++;
			uint16_t hi = cpu_read(cpu.PC);
			cpu.PC++;
		
			cpuAddrAbs = (hi << 8) | lo;
		
			return 0;
		}
	
	/**
	* Address Mode: Absolute with X Offset
	* Fundamentally the same as absolute addressing, but the contents of the X Register
	* is added to the supplied two byte address. If the resulting address changes
	* the page, an additional clock cycle is required
	* @return uint8_t
	*/
		uint8_t cpu_ABX(){
			uint16_t lo = cpu_read(cpu.PC);
			cpu.PC++;
			uint16_t hi = cpu_read(cpu.PC);
			cpu.PC++;
		
			cpuAddrAbs = (hi << 8) | lo;
			cpuAddrAbs += cpu.X;
		
			if ((cpuAddrAbs & 0xFF00) != (hi << 8))
				return 1;
			else
				return 0;
		}
	
	/**
	* Address Mode: Absolute with Y Offset
	* Fundamentally the same as absolute addressing, but the contents of the Y Register
	* is added to the supplied two byte address. If the resulting address changes
	* the page, an additional clock cycle is required
	* @return uint8_t
	*/
		uint8_t cpu_ABY(){
			uint16_t lo = cpu_read(cpu.PC);
			cpu.PC++;
			uint16_t hi = cpu_read(cpu.PC);
			cpu.PC++;
		
			cpuAddrAbs = (hi << 8) | lo;
			cpuAddrAbs += cpu.Y;
		
			if ((cpuAddrAbs & 0xFF00) != (hi << 8))
				return 1;
			else
				return 0;
		}
	
	// Note: The next 3 address modes use indirection (aka Pointers!)
	
	/**
	* Address Mode: Indirect
	* The supplied 16-bit address is read to get the actual 16-bit address. This is
	* instruction is unusual in that it has a bug in the hardware! To emulate its
	* function accurately, we also need to emulate this bug. If the low byte of the
	* supplied address is 0xFF, then to read the high byte of the actual address
	* we need to cross a page boundary. This doesnt actually work on the chip as
	* designed, instead it wraps back around in the same page, yielding an
	* invalid actual address
	* @return uint8_t
	*/
		uint8_t cpu_IND(){
			uint16_t ptr_lo = cpu_read(cpu.PC);
			cpu.PC++;
			uint16_t ptr_hi = cpu_read(cpu.PC);
			cpu.PC++;
		
			uint16_t ptr = (ptr_hi << 8) | ptr_lo;
		
			if (ptr_lo == 0x00FF) // Simulate page boundary hardware bug
			{
				cpuAddrAbs = (cpu_read(ptr & 0xFF00) << 8) | cpu_read(ptr + 0);
			}
			else // Behave normally
			{
				cpuAddrAbs = (cpu_read(ptr + 1) << 8) | cpu_read(ptr + 0);
			}
			
			return 0;
		}
	
	/**
	* Address Mode: Indirect X
	* The supplied 8-bit address is offset by X Register to index
	* a location in page 0x00. The actual 16-bit address is read
	* from this location
	* @return uint8_t
	*/
		uint8_t cpu_IZX(){
			uint16_t t = cpu_read(cpu.PC);
			cpu.PC++;
		
			uint16_t lo = cpu_read((uint16_t)(t + (uint16_t)cpu.X) & 0x00FF);
			uint16_t hi = cpu_read((uint16_t)(t + (uint16_t)cpu.X + 1) & 0x00FF);
		
			cpuAddrAbs = (hi << 8) | lo;
			
			return 0;
		}
	
	/**
	* Address Mode: Indirect Y
	* The supplied 8-bit address indexes a location in page 0x00. From
	* here the actual 16-bit address is read, and the contents of
	* Y Register is added to it to offset it. If the offset causes a
	* change in page then an additional clock cycle is required.
	* @return uint8_t
	*/
		uint8_t cpu_IZY(){
			uint16_t temp = cpu_read(cpu.PC);
			cpu.PC++;
		
			uint16_t lo = cpu_read(temp & 0x00FF);
			uint16_t hi = cpu_read((temp + 1) & 0x00FF);
		
			cpuAddrAbs = (hi << 8) | lo;
			cpuAddrAbs += cpu.Y;
			
			if ((cpuAddrAbs & 0xFF00) != (hi << 8))
				return 1;
			else
				return 0;
		}

///////////////////////////////////////////////////////////////////////////////
// INSTRUCTION IMPLEMENTATIONS

	/**
	* Note: Ive started with the two most complicated instructions to emulate, which
	* ironically is addition and subtraction! Ive tried to include a detailed
	* explanation as to why they are so complex, yet so fundamental. Im also NOT
	* going to do this through the explanation of 1 and 2's complement.
	* */
	
	/**
	* Instruction: Add with Carry In
	* Function:    A = A + M + C
	* Flags Out:   C, V, N, Z
	*
	* Explanation:
	* The purpose of this function is to add a value to the accumulator and a carry bit. If
	* the result is > 255 there is an overflow setting the carry bit. Ths allows you to
	* chain together ADC instructions to add numbers larger than 8-bits. This in itself is
	* simple, however the 6502 supports the concepts of Negativity/Positivity and Signed Overflow.
	*
	* 10000100 = 128 + 4 = 132 in normal circumstances, we know this as unsigned and it allows
	* us to represent numbers between 0 and 255 (given 8 bits). The 6502 can also interpret
	* this word as something else if we assume those 8 bits represent the range -128 to +127,
	* i.e. it has become signed.
	*
	* Since 132 > 127, it effectively wraps around, through -128, to -124. This wraparound is
	* called overflow, and this is a useful to know as it indicates that the calculation has
	* gone outside the permissable range, and therefore no longer makes numeric sense.
	*
	* Note the implementation of ADD is the same in binary, this is just about how the numbers
	* are represented, so the word 10000100 can be both -124 and 132 depending upon the
	* context the programming is using it in. We can prove this!
	*
	*  10000100 =  132  or  -124
	* +00010001 = + 17      + 17
	*  ========    ===       ===     See, both are valid additions, but our interpretation of
	*  10010101 =  149  or  -107     the context changes the value, not the hardware!
	*
	* In principle under the -128 to 127 range:
	* 10000000 = -128, 11111111 = -1, 00000000 = 0, 00000000 = +1, 01111111 = +127
	* therefore negative numbers have the most significant set, positive numbers do not
	*
	* To assist us, the 6502 can set the overflow flag, if the result of the addition has
	* wrapped around. V <- ~(A^M) & A^(A+M+C) :D lol, let's work out why!
	*
	* Let's suppose we have A = 30, M = 10 and C = 0
	*          A = 30 = 00011110
	*          M = 10 = 00001010+
	*     RESULT = 40 = 00101000
	*
	* Here we have not gone out of range. The resulting significant bit has not changed.
	* So let's make a truth table to understand when overflow has occurred. Here I take
	* the MSB of each component, where R is RESULT.
	*
	* A  M  R | V | A^R | A^M |~(A^M) |
	* 0  0  0 | 0 |  0  |  0  |   1   |
	* 0  0  1 | 1 |  1  |  0  |   1   |
	* 0  1  0 | 0 |  0  |  1  |   0   |
	* 0  1  1 | 0 |  1  |  1  |   0   |  so V = ~(A^M) & (A^R)
	* 1  0  0 | 0 |  1  |  1  |   0   |
	* 1  0  1 | 0 |  0  |  1  |   0   |
	* 1  1  0 | 1 |  1  |  0  |   1   |
	* 1  1  1 | 0 |  0  |  0  |   1   |
	*
	* We can see how the above equation calculates V, based on A, M and R. V was chosen
	* based on the following hypothesis:
	*       Positive Number + Positive Number = Negative Result -> Overflow
	*       Negative Number + Negative Number = Positive Result -> Overflow
	*       Positive Number + Negative Number = Either Result -> Cannot Overflow
	*       Positive Number + Positive Number = Positive Result -> OK! No Overflow
	*       Negative Number + Negative Number = Negative Result -> OK! NO Overflow
	* @return uint8_t
	*/
	
		uint8_t cpu_ADC(){
		
			// Grab the data that we are adding to the accumulator
				cpu_fetch();
			
			// Add is performed in 16-bit domain for emulation to capture any
			// carry bit, which will exist in bit 8 of the 16-bit word
				cpuTemp = (uint16_t)cpu.A + (uint16_t)cpuFetched + (uint16_t)cpu_get_flag(C);
			
			// The carry flag out exists in the high byte bit 0
				cpu_set_flag(C, cpuTemp > 255);
			
			// The Zero flag is set if the result is 0
				cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0);
			
			// The signed Overflow flag is set based on all that up there! :D
				cpu_set_flag(V, (~((uint16_t)cpu.A ^ (uint16_t)cpuFetched) & ((uint16_t)cpu.A ^ (uint16_t)cpuTemp)) & 0x0080);
			
			// The negative flag is set to the most significant bit of the result
				cpu_set_flag(N, cpuTemp & 0x80);
			
			// Load the result into the accumulator (it's 8-bit dont forget!)
				cpu_set_a(cpuTemp & 0x00FF);
			
			// This instruction has the potential to require an additional clock cycle
				return 1;
				
		}
	
	/**
	* Instruction: Subtraction with Borrow In
	* Function:    A = A - M - (1 - C)
	* Flags Out:   C, V, N, Z
	*
	* Explanation:
	* Given the explanation for ADC above, we can reorganise our data
	* to use the same computation for addition, for subtraction by multiplying
	* the data by -1, i.e. make it negative
	*
	* A = A - M - (1 - C)  ->  A = A + -1 * (M - (1 - C))  ->  A = A + (-M + 1 + C)
	*
	* To make a signed positive number negative, we can invert the bits and add 1
	* (OK, I lied, a little bit of 1 and 2s complement :P)
	*
	*  5 = 00000101
	* -5 = 11111010 + 00000001 = 11111011 (or 251 in our 0 to 255 range)
	*
	* The range is actually unimportant, because if I take the value 15, and add 251
	* to it, given we wrap around at 256, the result is 10, so it has effectively
	* subtracted 5, which was the original intention. (15 + 251) % 256 = 10
	*
	* Note that the equation above used (1-C), but this got converted to + 1 + C.
	* This means we already have the +1, so all we need to do is invert the bits
	* of M, the data(!) therfore we can simply add, exactly the same way we did
	* before.
	* @return uint8_t
	*/
		uint8_t cpu_SBC(){
		
			cpu_fetch();
			
			// Operating in 16-bit domain to capture carry out
			
			// We can invert the bottom 8 bits with bitwise xor
				uint16_t value = ((uint16_t)cpuFetched) ^ 0x00FF;
			
			// Notice this is exactly the same as addition from here!
			//should we cpu_get_flag silent here??
				cpuTemp = (uint16_t)cpu.A + value + (uint16_t)cpu_get_flag(C);
				cpu_set_flag(C, cpuTemp & 0xFF00);
				cpu_set_flag(Z, ((cpuTemp & 0x00FF) == 0));
				cpu_set_flag(V, (cpuTemp ^ (uint16_t)cpu.A) & (cpuTemp ^ value) & 0x0080);
				cpu_set_flag(N, cpuTemp & 0x0080);
				cpu_set_a(cpuTemp & 0x00FF);
				return 1;
				
		}
	
	/**
	* OK! Complicated operations are done! the following are much simpler
	* and conventional. The typical order of events is:
	* 1) Fetch the data you are working with
	* 2) Perform calculation
	* 3) Store the result in desired place
	* 4) Set Flags of the cpu.status register
	* 5) Return if instruction has potential to require additional
	*    clock cycle
	*    */
	
	/**
	* Instruction: Bitwise Logic AND
	* Function:    A = A & M
	* Flags Out:   N, Z
	* @return uint8_t
	*/
		uint8_t cpu_AND(){
		
			cpu_fetch();
			cpu_set_a(cpu.A & cpuFetched);
			cpu_set_flag(Z, cpu.A == 0x00);
			cpu_set_flag(N, cpu.A & 0x80);
			return 1;
			
		}
	
	/**
	* Instruction: Arithmetic Shift Left
	* Function:    A = C <- (A
	* @return uint8_t
	*/
		uint8_t cpu_ASL(){
			cpu_fetch();
			cpuTemp = (uint16_t)cpuFetched << 1;
			cpu_set_flag(C, (cpuTemp & 0xFF00) > 0);
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x00);
			cpu_set_flag(N, cpuTemp & 0x80);
			if (lookupCpu[cpuOPCode].addrmode == &cpu_IMP)
				cpu_set_a(cpuTemp & 0x00FF);
			else
				cpu_write(cpuAddrAbs, cpuTemp & 0x00FF);
			return 0;
		}
	
	/**
	* Instruction: Branch if Carry Clear
	* Function:    if(C == 0) cpu.PC = address
	* @return uint8_t
	*/
		uint8_t cpu_BCC(){
			if (cpu_get_flag(C) == 0)
			{
				cpuCycles++;
				cpuAddrAbs = cpu.PC + cpuAddrRel;
				
				if((cpuAddrAbs & 0xFF00) != (cpu.PC & 0xFF00))
					cpuCycles++;
				
				cpu_set_pc(cpuAddrAbs);
			}
			return 0;
		}
	
	/**
	* Instruction: Branch if Carry Set
	* Function:    if(C == 1) cpu.PC = address
	* @return uint8_t
	*/
		uint8_t cpu_BCS(){
			if (cpu_get_flag(C) == 1)
			{
				cpuCycles++;
				cpuAddrAbs = cpu.PC + cpuAddrRel;
		
				if ((cpuAddrAbs & 0xFF00) != (cpu.PC & 0xFF00))
					cpuCycles++;
		
				cpu_set_pc(cpuAddrAbs);
			}
			return 0;
		}
	
	/**
	* Instruction: Branch if Equal
	* Function:    if(Z == 1) cpu.PC = address
	* @return uint8_t
	*/
		uint8_t cpu_BEQ(){
			if (cpu_get_flag(Z) == 1)
			{
				cpuCycles++;
				cpuAddrAbs = cpu.PC + cpuAddrRel;
		
				if ((cpuAddrAbs & 0xFF00) != (cpu.PC & 0xFF00))
					cpuCycles++;
		
				cpu_set_pc(cpuAddrAbs);
			}
			return 0;
		}
	
	/**
	* Instruction: Bit Test
	* This instructions is used to test if one or more bits are set in a target memory location.
	* Flags Out:   Z,N,V
	* @return uint8_t
	*/
		uint8_t cpu_BIT()
		{
			cpu_fetch();
			cpuTemp = cpu.A & cpuFetched;
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x00);
			cpu_set_flag(N, cpuFetched & (1 << 7));
			cpu_set_flag(V, cpuFetched & (1 << 6));
			return 0;
		}
	
	/**
	* Instruction: Branch if Negative
	* Function:    if(N == 1) cpu.PC = address
	* @return uint8_t
	*/
		uint8_t cpu_BMI(){
			if (cpu_get_flag(N) == 1)
			{
				cpuCycles++;
				cpuAddrAbs = cpu.PC + cpuAddrRel;
		
				if ((cpuAddrAbs & 0xFF00) != (cpu.PC & 0xFF00))
					cpuCycles++;
		
				cpu_set_pc(cpuAddrAbs);
			}
			return 0;
		}
	
	/**
	* Instruction: Branch if Not Equal
	* Function:    if(Z == 0) cpu.PC = address
	* @return uint8_t
	*/
		uint8_t cpu_BNE(){
			if (cpu_get_flag(Z) == 0)
			{
				cpuCycles++;
				cpuAddrAbs = cpu.PC + cpuAddrRel;
		
				if ((cpuAddrAbs & 0xFF00) != (cpu.PC & 0xFF00))
					cpuCycles++;
		
				cpu_set_pc(cpuAddrAbs);
			}
			return 0;
		}
	
	/**
	* Instruction: Branch if Positive
	* Function:    if(N == 0) cpu.PC = address
	* @return uint8_t
	*/
		uint8_t cpu_BPL(){
			if (cpu_get_flag(N) == 0)
			{
				cpuCycles++;
				cpuAddrAbs = cpu.PC + cpuAddrRel;
		
				if ((cpuAddrAbs & 0xFF00) != (cpu.PC & 0xFF00))
					cpuCycles++;
		
				cpu_set_pc(cpuAddrAbs);
			}
			return 0;
		}

	/**
	* Instruction: Break
	* Function:    Program Sourced Interrupt
	* * @return uint8_t
	*/
		uint8_t cpu_BRK(){
			cpu.PC++;
			
			cpu_set_flag(I, 1);
			cpu_write(0x0100 + cpu.SP, (cpu.PC >> 8) & 0x00FF);
			cpu.SP--;
			cpu_write(0x0100 + cpu.SP, cpu.PC & 0x00FF);
			cpu.SP--;
		
			cpu_set_flag(B, 1);
			cpu_write(0x0100 + cpu.SP, cpu.status);
			cpu.SP--;
			cpu_set_flag(B, 0);
		
			cpu_set_pc(cpu_read(0xFFFE) | (cpu_read(0xFFFF) << 8));
			return 0;
		}
	
	/**
	* Instruction: Branch if Overflow Clear
	* Function:    if(V == 0) cpu.PC = address
	* * @return uint8_t
	*/
		uint8_t cpu_BVC(){
			if (cpu_get_flag(V) == 0)
			{
				cpuCycles++;
				cpuAddrAbs = cpu.PC + cpuAddrRel;
		
				if ((cpuAddrAbs & 0xFF00) != (cpu.PC & 0xFF00))
					cpuCycles++;
		
				cpu_set_pc(cpuAddrAbs);
			}
			return 0;
		}
	
	/**
	* Instruction: Branch if Overflow Set
	* Function:    if(V == 1) cpu.PC = address
	* * @return uint8_t
	*/
		uint8_t cpu_BVS(){
			if (cpu_get_flag(V) == 1)
			{
				cpuCycles++;
				cpuAddrAbs = cpu.PC + cpuAddrRel;
		
				if ((cpuAddrAbs & 0xFF00) != (cpu.PC & 0xFF00))
					cpuCycles++;
		
				cpu_set_pc(cpuAddrAbs);
			}
			return 0;
		}
	
	/**
	* Instruction: Clear Carry Flag
	* Function:    C = 0
	* * @return uint8_t
	*/
		uint8_t cpu_CLC(){
			cpu_set_flag(C, false);
			return 0;
		}
	
	/**
	* Instruction: Clear Decimal Flag
	* Function:    D = 0
	* * @return uint8_t
	*/
		uint8_t cpu_CLD(){
			cpu_set_flag(D, false);
			return 0;
		}
	
	/**
	* Instruction: Disable Interrupts / Clear Interrupt Flag
	* Function:    I = 0
	* * @return uint8_t
	*/
		uint8_t cpu_CLI(){
			cpu_set_flag(I, false);
			return 0;
		}
	
	/**
	* Instruction: Clear Overflow Flag
	* Function:    V = 0
	* * @return uint8_t
	*/
		uint8_t cpu_CLV(){
			cpu_set_flag(V, false);
			return 0;
		}
	
	/**
	* Instruction: Compare Accumulator
	* Function:    C <- A >= M      Z <- (A - M) == 0
	* Flags Out:   N, C, Z
	* * @return uint8_t
	*/
		uint8_t cpu_CMP(){
			cpu_fetch();
			cpuTemp = (uint16_t)cpu.A - (uint16_t)cpuFetched;
			cpu_set_flag(C, cpu.A >= cpuFetched);
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x0000);
			cpu_set_flag(N, cpuTemp & 0x0080);
			return 1;
		}
	
	/**
	* Instruction: Compare X Register
	* Function:    C <- X >= M      Z <- (X - M) == 0
	* Flags Out:   N, C, Z
	* * @return uint8_t
	*/
		uint8_t cpu_CPX(){
			cpu_fetch();
			cpuTemp = (uint16_t)cpu.X - (uint16_t)cpuFetched;
			cpu_set_flag(C, cpu.X >= cpuFetched);
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x0000);
			cpu_set_flag(N, cpuTemp & 0x0080);
			return 0;
		}
	
	/**
	* Instruction: Compare Y Register
	* Function:    C <- Y >= M      Z <- (Y - M) == 0
	* Flags Out:   N, C, Z
	* * @return uint8_t
	*/
		uint8_t cpu_CPY(){
			cpu_fetch();
			cpuTemp = (uint16_t)cpu.Y - (uint16_t)cpuFetched;
			cpu_set_flag(C, cpu.Y >= cpuFetched);
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x0000);
			cpu_set_flag(N, cpuTemp & 0x0080);
			return 0;
		}
	
	/**
	* Instruction: Decrement Value at Memory Location
	* Function:    M = M - 1
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_DEC(){
			cpu_fetch();
			cpuTemp = cpuFetched - 1;
			cpu_write(cpuAddrAbs, cpuTemp & 0x00FF);
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x0000);
			cpu_set_flag(N, cpuTemp & 0x0080);
			return 0;
		}
	
	/**
	* Instruction: Decrement X Register
	* Function:    X = X - 1
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_DEX(){
			cpu.X--;
			cpu_set_flag(Z, cpu.X == 0x00);
			cpu_set_flag(N, cpu.X & 0x80);
			return 0;
		}
	
	/**
	* Instruction: Decrement Y Register
	* Function:    Y = Y - 1
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_DEY(){
			cpu.Y--;
			cpu_set_flag(Z, cpu.Y == 0x00);
			cpu_set_flag(N, cpu.Y & 0x80);
			return 0;
		}
	
	/**
	* Instruction: Bitwise Logic XOR
	* Function:    A = A xor M
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_EOR(){
			cpu_fetch();
			cpu_set_a(cpu.A ^ cpuFetched);
			cpu_set_flag(Z, cpu.A == 0x00);
			cpu_set_flag(N, cpu.A & 0x80);
			return 1;
		}
	
	/**
	* Instruction: Increment Value at Memory Location
	* Function:    M = M + 1
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_INC(){
			cpu_fetch();
			cpuTemp = cpuFetched + 1;
			cpu_write(cpuAddrAbs, cpuTemp & 0x00FF);
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x0000);
			cpu_set_flag(N, cpuTemp & 0x0080);
			return 0;
		}
	
	/**
	* Instruction: Increment X Register
	* Function:    X = X + 1
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_INX(){
			cpu.X++;
			cpu_set_flag(Z, cpu.X == 0x00);
			cpu_set_flag(N, cpu.X & 0x80);
			return 0;
		}
	
	/**
	* Instruction: Increment Y Register
	* Function:    Y = Y + 1
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_INY(){
			cpu.Y++;
			cpu_set_flag(Z, cpu.Y == 0x00);
			cpu_set_flag(N, cpu.Y & 0x80);
			return 0;
		}
	
	/**
	* Instruction: Jump To Location
	* Function:    cpu.PC = address
	* * @return uint8_t
	*/
		uint8_t cpu_JMP(){
			cpu_set_pc(cpuAddrAbs);
			return 0;
		}
		
	/**
	* Instruction: Jump To Sub-Routine
	* Function:    Push current cpu.PC to stack, cpu.PC = address
	* * @return uint8_t
	*/
		uint8_t cpu_JSR(){
			cpu.PC--;
		
			cpu_write(0x0100 + cpu.SP, (cpu.PC >> 8) & 0x00FF);
			cpu.SP--;
			cpu_write(0x0100 + cpu.SP, cpu.PC & 0x00FF);
			cpu.SP--;
		
			cpu_set_pc(cpuAddrAbs);
			return 0;
		}
		
	/**
	* Instruction: Load The Accumulator
	* Function:    A = M
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_LDA(){
			cpu_fetch();
			cpu_set_a(cpuFetched);
			cpu_set_flag(Z, cpu.A == 0x00);
			cpu_set_flag(N, cpu.A & 0x80);
			return 1;
		}
	
	/**
	* Instruction: Load The X Register
	* Function:    X = M
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_LDX(){
			cpu_set_x(cpu_fetch());
			cpu_set_flag(Z, cpu.X == 0x00);
			cpu_set_flag(N, cpu.X & 0x80);
			return 1;
		}
	
	/**
	* Instruction: Load The Y Register
	* Function:    Y = M
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_LDY(){
			cpu_set_y(cpu_fetch());
			cpu_set_flag(Z, cpu.Y == 0x00);
			cpu_set_flag(N, cpu.Y & 0x80);
			return 1;
		}
	
	/**
	* Instruction: Logical Shift Right
	* @return uint8_t
	*/
		uint8_t cpu_LSR(){
			cpu_fetch();
			cpu_set_flag(C, cpuFetched & 0x0001);
			
			cpuTemp = cpuFetched >> 1;
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x0000);
			cpu_set_flag(N, cpuTemp & 0x0080);
			
			printf("---Set V,Z,N\n");
			
			if (lookupCpu[cpuOPCode].addrmode == &cpu_IMP){
				cpu_set_a(cpuTemp & 0x00FF);
				printf("---A set to %#02X [%d]\n", cpu.A, cpu.A);
			} else {
				cpu_write(cpuAddrAbs, cpuTemp & 0x00FF);
				printf("---Memory %#04X set to %#02X [%d]\n", cpuAddrAbs, cpuTemp & 0x00FF, cpuTemp & 0x00FF);
			}
			return 0;
		}
	
	/**
	* No Operation
	* @return
	*/
		uint8_t cpu_NOP(){
			// Sadly not all NOPs are equal, Ive added a few here
			// based on https://wiki.nesdev.com/w/index.php/CPU_unofficial_cpuOPCodes
			// and will add more based on game compatibility, and ultimately
			// I'd like to cover all illegal cpuOPCodes too
			printf("---NOP\n");
			switch (cpuOPCode) {
				case 0x1C:
				case 0x3C:
				case 0x5C:
				case 0x7C:
				case 0xDC:
				case 0xFC:
					return 1;
				default:
					return 0;
			}
			
		}
	
	/**
	* Instruction: Bitwise Logic OR
	* Function:    A = A | M
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_ORA(){
			cpu_set_a(cpu.A | cpu_fetch());
			printf("---A OR'd to %#02X and set Z,N\n", cpu.A);
			cpu_set_flag(Z, cpu.A == 0x00);
			cpu_set_flag(N, cpu.A & 0x80);
			return 1;
		}
	
	/**
	* Instruction: Push Accumulator to Stack
	* Function:    A -> stack
	* * @return uint8_t
	*/
		uint8_t cpu_PHA(){
			cpu_write(0x0100 + cpu.SP, cpu.A);
			printf("---Pushed %#02X [%d] to stack\n", cpu.A, cpu.A);
			cpu.SP--;
			return 0;
		}
	
	/**
	* Instruction: Push Status Register to Stack
	* Function:    cpu.status -> stack
	* Note:        Break flag is set to 1 before push
	* * @return uint8_t
	*/
		uint8_t cpu_PHP(){
			cpu_write(0x0100 + cpu.SP, cpu.status | B | U);
			printf("---Pushed %#02X [%d] to stack and set B,U\n", cpu.status | B | U, cpu.status | B | U);
			cpu_set_flag(B, 0);
			cpu_set_flag(U, 0);
			cpu.SP--;
			return 0;
		}
	
	/**
	* Instruction: Pop Accumulator off Stack
	* Function:    A <- stack
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_PLA(){
			cpu.SP++;
			cpu_set_a(cpu_read(0x0100 + cpu.SP));
			printf("---Popped %#02X [%d] to A and set Z,N\n", cpu.A, cpu.A);
			cpu_set_flag(Z, cpu.A == 0x00);
			cpu_set_flag(N, cpu.A & 0x80);
			return 0;
		}
	
	/**
	* Instruction: Pop Status Register off Stack
	* Function:    Status <- stack
	* * @return uint8_t
	*/
		uint8_t cpu_PLP(){
			cpu.SP++;
			cpu.status = cpu_read(0x0100 + cpu.SP);
			printf("---Popped %#02X [%d] to status\n", cpu.status, cpu.status);
			cpu_set_flag(U, 1);
			return 0;
		}
	
	/**
	* Instruction: Rotate right
	* Move each of the bits in either A or M one place to the right.
	* Bit 7 is filled with the current value of the carry flag whilst the old bit 0 becomes the new carry flag value.
	* Flags Out:   C,Z,N
	* @return uint8_t
	*/
		uint8_t cpu_ROL(){
			cpu_fetch();
			cpuTemp = (uint16_t)(cpuFetched << 1) | cpu_get_flag(C);
			cpu_set_flag(C, cpuTemp & 0xFF00);
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x0000);
			cpu_set_flag(N, cpuTemp & 0x0080);
			if (lookupCpu[cpuOPCode].addrmode == &cpu_IMP)
				cpu_set_a(cpuTemp & 0x00FF);
			else
				cpu_write(cpuAddrAbs, cpuTemp & 0x00FF);
			return 0;
		}
	
	/**
	* Instruction: Rotate right
	* Move each of the bits in either A or M one place to the right.
	* Bit 7 is filled with the current value of the carry flag whilst the old bit 0 becomes the new carry flag value.
	* Flags Out:   C,Z,N
	* @return uint8_t
	*/
		uint8_t cpu_ROR(){
			cpu_fetch();
			cpuTemp = (uint16_t)(cpu_get_flag(C) << 7) | (cpuFetched >> 1);
			cpu_set_flag(C, cpuFetched & 0x01);
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x00);
			cpu_set_flag(N, cpuTemp & 0x0080);
			if (lookupCpu[cpuOPCode].addrmode == &cpu_IMP)
				cpu_set_a(cpuTemp & 0x00FF);
			else
				cpu_write(cpuAddrAbs, cpuTemp & 0x00FF);
			return 0;
		}
	
	/**
	* Instruction: Return from Interrupt
	* @return uint8_t
	*/
		uint8_t cpu_RTI(){
			cpu.SP++;
			cpu.status = cpu_read(0x0100 + cpu.SP);
			cpu.status &= ~B;
			cpu.status &= ~U;
		
			cpu.SP++;
			cpu_set_pc(cpu_read(0x0100 + cpu.SP));
			cpu.SP++;
			cpu.PC |= cpu_read(0x0100 + cpu.SP) << 8;
			return 0;
		}
	
	/**
	* Instruction: Return from subroutine
	* @return uint8_t
	*/
		uint8_t cpu_RTS(){
			cpu.SP++;
			cpu_set_pc(cpu_read(0x0100 + cpu.SP));
			cpu.SP++;
			cpu.PC |= cpu_read(0x0100 + cpu.SP) << 8;
			
			cpu.PC++;
			return 0;
		}
	
	/**
	* Instruction: Set Carry Flag
	* Function:    C = 1
	* * @return uint8_t
	*/
		uint8_t cpu_SEC(){
			cpu_set_flag(C, true);
			printf("---Set C\n");
			return 0;
		}
	
	/**
	* Instruction: Set Decimal Flag
	* Function:    D = 1
	* * @return uint8_t
	*/
		uint8_t cpu_SED(){
			cpu_set_flag(D, true);
			printf("---Set D\n");
			return 0;
		}
	
	/**
	* Instruction: Set Interrupt Flag / Enable Interrupts
	* Function:    I = 1
	* * @return uint8_t
	*/
		uint8_t cpu_SEI(){
			cpu_set_flag(I, true);
			printf("---Set I\n");
			return 0;
		}
	
	/**
	* Instruction: Store Accumulator at Address
	* Function:    M = A
	* * @return uint8_t
	*/
		uint8_t cpu_STA(){
			cpu_write(cpuAddrAbs, cpu.A);
			return 0;
		}
	
	/**
	* Instruction: Store X Register at Address
	* Function:    M = X
	* * @return uint8_t
	*/
		uint8_t cpu_STX(){
			cpu_write(cpuAddrAbs, cpu.X);
			return 0;
		}
	
	/**
	* Instruction: Store Y Register at Address
	* Function:    M = Y
	* * @return uint8_t
	*/
		uint8_t cpu_STY(){
			cpu_write(cpuAddrAbs, cpu.Y);
			return 0;
		}
	
	/**
	* Instruction: Transfer Accumulator to X Register
	* Function:    X = A
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_TAX(){
			cpu_set_x(cpu.A);
			cpu_set_flag(Z, cpu.X == 0x00);
			cpu_set_flag(N, cpu.X & 0x80);
			printf("---Transferred %#02X [%d] to X and set Z,N\n", cpu.A, cpu.A);
			return 0;
		}
	
	/**
	* Instruction: Transfer Accumulator to Y Register
	* Function:    Y = A
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_TAY(){
			cpu_set_y(cpu.A);
			cpu_set_flag(Z, cpu.Y == 0x00);
			cpu_set_flag(N, cpu.Y & 0x80);
			printf("---Transferred %#02X [%d] to Y and set Z,N\n", cpu.A, cpu.A);
			return 0;
		}
	
	/**
	* Instruction: Transfer Stack Pointer to X Register
	* Function:    X = stack pointer
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_TSX(){
			cpu_set_x(cpu.SP);
			cpu_set_flag(Z, cpu.X == 0x00);
			cpu_set_flag(N, cpu.X & 0x80);
			printf("---Transferred %#02X [%d] to X and set Z,N\n", cpu.SP, cpu.SP);
			return 0;
		}
	
	/**
	* Instruction: Transfer X Register to Accumulator
	* Function:    A = X
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_TXA(){
			cpu_set_a(cpu.X);
			cpu_set_flag(Z, cpu.A == 0x00);
			cpu_set_flag(N, cpu.A & 0x80);
			printf("---Transferred %#02X [%d] to A and set Z,N\n", cpu.X, cpu.X);
			return 0;
		}
	
	/**
	* Instruction: Transfer X Register to Stack Pointer
	* Function:    stack pointer = X
	* * @return uint8_t
	*/
		uint8_t cpu_TXS(){
			cpu_set_sp(cpu.X);
			printf("---Transferred %#02X [%d] to SP\n", cpu.X, cpu.X);
			return 0;
		}
		
	/**
	* Instruction: Transfer Y Register to Accumulator
	* Function:    A = Y
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_TYA(){
			cpu_set_a(cpu.Y);
			cpu_set_flag(Z, cpu.A == 0x00);
			cpu_set_flag(N, cpu.A & 0x80);
			printf("---Transferred %#02X [%d] to A and set Z,N\n", cpu.Y, cpu.Y);
			return 0;
		}
	
	/**
	* This function captures illegal cpuOPCodes
	* @return uint8_t
	*/
		uint8_t cpu_XXX(){
			return 0;
		}
		
	/*
	* This is the disassembly function. Its workings are not required for emulation.
	* It is merely a convenience function to turn the binary instruction code into
	* human readable form. Its included as part of the emulator because it can take
	* advantage of many of the CPUs internal operations to do this.
	* @return void
	*/
		void disassemble(uint16_t nStart, uint16_t nStop)
		{
		
			//set starting address casting to uint32
			uint32_t addr = (uint32_t)nStart;
			
			//some helper vars
			uint8_t value = 0x00, lo = 0x00, hi = 0x00;
			
			uint16_t line_addr = 0;
		
			// Starting at the specified address we read an instruction
			// byte, which in turn yields information from the lookup table
			// as to how many additional bytes we need to read and what the
			// addressing mode is. I need this info to assemble human readable
			// syntax, which is different depending upon the addressing mode
		
			// As the instruction is decoded, a std::string is assembled
			// with the readable output
			while (addr <= (uint32_t)nStop)
			{
			
				line_addr = addr;
		
				// Prefix line with instruction address
					char line[50] = "0x";
					char hashTemp[5]; //+1 for \0
					hex(line_addr, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, ": ");
		
				// Read instruction, and get its readable name
				uint8_t opcode = bus_read((uint16_t)addr); addr++;
				strcat(line, lookupCpu[opcode].name);
				strcat(line, "\t");
		
				// Get oprands from desired locations, and form the
				// instruction based upon its addressing mode. These
				// routines mimmick the actual fetch routine of the
				// 6502 in order to get accurate data as part of the
				// instruction
				if (lookupCpu[opcode].addrmode == &cpu_IMP){
				
					strcat(line, "\t{IMP}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_IMM){
				
					strcpy(code[addr], "---");
					value = bus_read(addr); addr++;
					strcat(line, "#0x");
					hex(value, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, "\t{IMM}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_ZP0){
				
					strcpy(code[addr], "---");
					lo = bus_read(addr); addr++;
					hi = 0x00;
					strcat(line, "0x");
					hex(lo, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, "\t{ZP0}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_ZPX){
				
					strcpy(code[addr], "---");
					lo = bus_read(addr); addr++;
					hi = 0x00;
					strcat(line, "0x");
					hex(lo, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, ", X\t{ZPX}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_ZPY){
				
					strcpy(code[addr], "---");
					lo = bus_read(addr); addr++;
					hi = 0x00;
					strcat(line, "0x");
					hex(lo, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, ", Y\t{ZPY}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_IZX){
				
					strcpy(code[addr], "---");
					lo = bus_read(addr); addr++;
					hi = 0x00;
					strcat(line, "(0x");
					hex(lo, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, ", X)\t{IZX}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_IZY){
				
					strcpy(code[addr], "---");
					lo = bus_read(addr); addr++;
					hi = 0x00;
					strcat(line, "(0x");
					hex(lo, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, "), Y\t{IZY}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_ABS){
				
					strcpy(code[addr], "---");
					lo = bus_read(addr); addr++;
					strcpy(code[addr], "---");
					hi = bus_read(addr); addr++;
					strcat(line, "$0x");
					hex((uint16_t)(hi << 8) | lo, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, "\t{ABS}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_ABX){
				
					strcpy(code[addr], "---");
					lo = bus_read(addr); addr++;
					strcpy(code[addr], "---");
					hi = bus_read(addr); addr++;
					strcat(line, "$0x");
					hex((uint16_t)(hi << 8) | lo, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, ", X\t{ABX}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_ABY){
				
					strcpy(code[addr], "---");
					lo = bus_read(addr); addr++;
					strcpy(code[addr], "---");
					hi = bus_read(addr); addr++;
					strcat(line, "$0x");
					hex((uint16_t)(hi << 8) | lo, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, ", Y\t{ABY}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_IND){
				
					strcpy(code[addr], "---");
					lo = bus_read(addr); addr++;
					strcpy(code[addr], "---");
					hi = bus_read(addr); addr++;
					strcat(line, "(0x");
					hex((uint16_t)(hi << 8) | lo, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, ")\t{IND}");
					
				} else if (lookupCpu[opcode].addrmode == &cpu_REL){
				
					strcpy(code[addr], "---");
					value = bus_read(addr); addr++;
					strcat(line, "(0x");
					hex(value, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, ") [0x");
					hex(addr + value, 4, hashTemp);
					strcat(line, hashTemp);
					strcat(line, "]\t{REL}");
					
				}
		
				// Add the formed string to code[], using the instruction's
				// address as the key. This makes it convenient to look for later
				// as the instructions are variable in length, so a straight up
				// incremental index is not sufficient.
				strcpy(code[line_addr], line);
				
			}
			
			printf("%sProgram Disassembled.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
			
		}

	/**
	* Draw all system output types to console
	* @return void
	*/
		void drawAll(){
			printf("\n");
			code_draw(cpuLastOpAddr, 7);
			printf("\n");
			cpu_draw();
			printf("\n\n");
			ram_draw(0x0000, 1, 16);
			printf("\n");
			ram_draw(0x8000, 2, 16);
			printf("\n");
			ram_draw(0x9000, 19, 16);
			printf("\n");
			dis_draw();
			printf("\n");
		}

int main(int argc, char *argv[])
{

	// If the first character of optstring is '-', then each nonoption argv-element is handled as if
	// it were the argument of an option with character code 1.
	// (This is used by programs that were written to expect options and other argv-elements in any order and that care about the ordering of the two.)
		bool outputAllOnExecute = false;
		bool outputMemOnExecute = false;
		bool outputCPUOnExecute = false;
		bool outputCodeOnExecute = false;
		bool outputDisOnExecute = false;
		int opt;
		while ((opt = getopt(argc, argv, "-mcpad")) != -1) {
			
			switch (opt) {
					case 'm':
						outputMemOnExecute = true;
					break;
					
					case 'c':
						outputCPUOnExecute = true;
					break;
					
					case 'p':
						outputCodeOnExecute = true;
					break;
					
					case 'a':
						outputAllOnExecute = true;
					break;
					
					case 'd':
						outputDisOnExecute = true;
					break;
					
				//option not in optstring
					case '?':
					default:
						printf("Option not in option list of [-mcpa]");
						exit(EXIT_FAILURE);
				
			}
			
		}

	printf("\n%s6502 C Emulator.%s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
	printf("\n%sPort and modification from OLC 6502 C++ emulator.%s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
	printf("\n%sPress Enter to step through each instruction.%s\n\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
	
	//add ram to bus
		bus_add_devices();
	
	//clear ram of any standing data
		ram_clear();
		
	//fill dissassemble array with starting state of memory (NOP's)
		code_fillBlank();
	
	//load program into ram
		ram_load_program();
		
	//disassemble into code array
		disassemble(programStart, programStart+programSize);
		
	printf("\n%s6502 Powered On.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
	
	//reset display
		dis_reset();
	
	//turn on the system
		cpu_reset();
	
	//draw any initial states from passed options
		if(outputAllOnExecute){
			drawAll();
		} else {
		
			printf("\n");
			
			if(outputCodeOnExecute){
				code_draw(cpuLastOpAddr, 9);
				printf("\n");
			}
			
			if(outputCPUOnExecute){
				cpu_draw();
				printf("\n\n");
			}
			
			if(outputMemOnExecute){
				ram_draw(0x0000, 2, 16);
				printf("\n");
				ram_draw(0x8000, 4, 16);
				printf("\n");
				ram_draw(0x9000, 19, 16);
				printf("\n");
			}
			
			if(outputDisOnExecute){
				dis_draw();
				printf("\n");
			}
		
		}
	
	//store input
		char consoleCommand[2] = " \0";
	
	//flag for ignoring \n at end of stdin when other chars entered
		bool ignoreNextEnter = false;
	
	while(cpu_running()){
	
		//wait for enter input
		if(fgets(consoleCommand,2,stdin) != NULL){
		
			//if only enter key pressed
				if(strcmp(&consoleCommand[0], "\n") == 0){
					if(ignoreNextEnter){
						ignoreNextEnter = false;
					} else {
						cpu_execute();
						dis_execute();
						if(outputAllOnExecute){
							drawAll();
						} else {
						
							printf("\n");
							
							if(outputCodeOnExecute){
								code_draw(cpuLastOpAddr, 9);
								printf("\n");
							}
							
							if(outputCPUOnExecute){
								cpu_draw();
								printf("\n\n");
							}
							
							if(outputMemOnExecute){
								ram_draw(0x0000, 2, 16);
								printf("\n");
								ram_draw(0x8000, 4, 16);
								printf("\n");
								ram_draw(0x9000, 19, 16);
								printf("\n");
							}
						
							if(outputDisOnExecute){
								dis_draw();
								printf("\n");
							}
						
						}
						
					}
				}
			
			//memory dump
				else if(strcmp(&consoleCommand[0], "m") == 0){
					printf("Memory Dump\n");
					ram_draw(0x0000, 2, 16);
					printf("\n");
					ram_draw(0x8000, 16, 16);
					printf("\n");
					ram_draw(0x9000, 19, 16);
					ignoreNextEnter = true;
				}
				
			//cpu dump
				else if(strcmp(&consoleCommand[0], "c") == 0){
					printf("CPU Dump\n");
					cpu_draw();
					printf("\n\n");
					ignoreNextEnter = true;
				}
				
			//stack dump
				else if(strcmp(&consoleCommand[0], "s") == 0){
					printf("Stack Dump\n");
					ignoreNextEnter = true;
				}
				
			//program dump
				else if(strcmp(&consoleCommand[0], "p") == 0){
					printf("Program Dump\n");
					code_draw(cpuLastOpAddr, 15);
					ignoreNextEnter = true;
				}
				
			//display dump
				else if(strcmp(&consoleCommand[0], "d") == 0){
					printf("Display Dump\n");
					dis_draw();
					ignoreNextEnter = true;
				}
				
			//all
				else if(strcmp(&consoleCommand[0], "a") == 0){
					drawAll();
					ignoreNextEnter = true;
				}
				
			//any other char bleeds round the enter key at the end of stdin so cpu_execute will fire
			
		}
		
	}
	
	return 0;
	
}
