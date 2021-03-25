#include "main.h"

char ANSI_COLOR_RED[] = "\x1b[31m";
char ANSI_COLOR_GREEN[] = "\x1b[32m";
char ANSI_COLOR_YELLOW[] = "\x1b[33m";
char ANSI_COLOR_BLUE[] = "\x1b[34m";
char ANSI_COLOR_MAGENTA[] = "\x1b[35m";
char ANSI_COLOR_CYAN[] = "\x1b[36m";
char ANSI_COLOR_RESET[] = "\x1b[0m";

RAM ram;
Bus bus;
CPU cpu;

// Represents the working input value to the ALU
	uint8_t cpuFetched = 0x00;
	
// A convenience variable used everywhere
	uint16_t cpuTemp  = 0x0000;
	
// All used memory addresses end up in here
	uint16_t cpuAddrAbs     = 0x0000;

// Represents absolute address following a branch
	uint16_t cpuAddrRel     = 0x00;

// Is the instruction byte
	uint8_t  cpuOPCode      = 0x00;
	
// Counts how many cpuCycles the instruction has remaining
	uint8_t  cpuCycles      = 0;
	
//ref to last loaded ins
	INSTRUCTION cpuIns;
	
//flag to say cycles just set
	bool cuCyclesInit = false;

// A global accumulation of the number of clocks
	uint32_t cpuClockCount  = 0;
	
//flag to see if we've run through memory or not
	bool cpuExecuting = true;
	
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
	INSTRUCTION lookupM[256] = {
		{"BRK", "Break Interrupt, Implied", &cpu_BRK, &cpu_IMM, 7 },	                {"ORA", "Or with A, X-indexed, Indirect", &cpu_ORA, &cpu_IZX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"???", "", &cpu_NOP, &cpu_IMP, 3 },	{"ORA", "", &cpu_ORA, &cpu_ZP0, 3 },	{"ASL", "", &cpu_ASL, &cpu_ZP0, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },	{"PHP", "", &cpu_PHP, &cpu_IMP, 3 },	{"ORA", "", &cpu_ORA, &cpu_IMM, 2 },	{"ASL", "", &cpu_ASL, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"ORA", "", &cpu_ORA, &cpu_ABS, 4 },	{"ASL", "", &cpu_ASL, &cpu_ABS, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },
		{"BPL", "Branch on Plus, Relative", &cpu_BPL, &cpu_REL, 2 },	                {"ORA", "Or with A, Y-indexed, Indirect", &cpu_ORA, &cpu_IZY, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"ORA", "", &cpu_ORA, &cpu_ZPX, 4 },	{"ASL", "", &cpu_ASL, &cpu_ZPX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },	{"CLC", "", &cpu_CLC, &cpu_IMP, 2 },	{"ORA", "", &cpu_ORA, &cpu_ABY, 4 },	{"???", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"ORA", "", &cpu_ORA, &cpu_ABX, 4 },	{"ASL", "", &cpu_ASL, &cpu_ABX, 7 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },
		{"JSR", "Jump Subroutine, Absolute", &cpu_JSR, &cpu_ABS, 6 },	            {"AND", "AND, X-indexed, Indirect", &cpu_AND, &cpu_IZX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"BIT", "", &cpu_BIT, &cpu_ZP0, 3 },	{"AND", "", &cpu_AND, &cpu_ZP0, 3 },	{"ROL", "", &cpu_ROL, &cpu_ZP0, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },	{"PLP", "", &cpu_PLP, &cpu_IMP, 4 },	{"AND", "", &cpu_AND, &cpu_IMM, 2 },	{"ROL", "", &cpu_ROL, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"BIT", "", &cpu_BIT, &cpu_ABS, 4 },	{"AND", "", &cpu_AND, &cpu_ABS, 4 },	{"ROL", "", &cpu_ROL, &cpu_ABS, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },
		{"BMI", "Branch on Minus, Relative", &cpu_BMI, &cpu_REL, 2 },	            {"AND", "AND, Y-indexed, Indirect", &cpu_AND, &cpu_IZY, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"AND", "", &cpu_AND, &cpu_ZPX, 4 },	{"ROL", "", &cpu_ROL, &cpu_ZPX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },	{"SEC", "", &cpu_SEC, &cpu_IMP, 2 },	{"AND", "", &cpu_AND, &cpu_ABY, 4 },	{"???", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"AND", "", &cpu_AND, &cpu_ABX, 4 },	{"ROL", "", &cpu_ROL, &cpu_ABX, 7 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },
		{"RTI", "Return from Interrupt, Implied", &cpu_RTI, &cpu_IMP, 6 },	        {"EOR", "Exclusive OR, X-indexed, Indirect", &cpu_EOR, &cpu_IZX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"???", "", &cpu_NOP, &cpu_IMP, 3 },	{"EOR", "", &cpu_EOR, &cpu_ZP0, 3 },	{"LSR", "", &cpu_LSR, &cpu_ZP0, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },	{"PHA", "", &cpu_PHA, &cpu_IMP, 3 },	{"EOR", "", &cpu_EOR, &cpu_IMM, 2 },	{"LSR", "", &cpu_LSR, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"JMP", "", &cpu_JMP, &cpu_ABS, 3 },	{"EOR", "", &cpu_EOR, &cpu_ABS, 4 },	{"LSR", "", &cpu_LSR, &cpu_ABS, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },
		{"BVC", "Branch on Overflow Clear, Relative", &cpu_BVC, &cpu_REL, 2 },	    {"EOR", "Exclusive OR, y-indexed, Indirect", &cpu_EOR, &cpu_IZY, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"EOR", "", &cpu_EOR, &cpu_ZPX, 4 },	{"LSR", "", &cpu_LSR, &cpu_ZPX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },	{"CLI", "", &cpu_CLI, &cpu_IMP, 2 },	{"EOR", "", &cpu_EOR, &cpu_ABY, 4 },	{"???", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"EOR", "", &cpu_EOR, &cpu_ABX, 4 },	{"LSR", "", &cpu_LSR, &cpu_ABX, 7 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },
		{"RTS", "Return from subroutine, Implied", &cpu_RTS, &cpu_IMP, 6 },	        {"ADC", "Add with Carry, X-indexed, Indirect", &cpu_ADC, &cpu_IZX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"???", "", &cpu_NOP, &cpu_IMP, 3 },	{"ADC", "", &cpu_ADC, &cpu_ZP0, 3 },	{"ROR", "", &cpu_ROR, &cpu_ZP0, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },	{"PLA", "", &cpu_PLA, &cpu_IMP, 4 },	{"ADC", "", &cpu_ADC, &cpu_IMM, 2 },	{"ROR", "", &cpu_ROR, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"JMP", "", &cpu_JMP, &cpu_IND, 5 },	{"ADC", "", &cpu_ADC, &cpu_ABS, 4 },	{"ROR", "", &cpu_ROR, &cpu_ABS, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },
		{"BVS", "Branch on Overflow Set, Relative", &cpu_BVS, &cpu_REL, 2 },	        {"ADC", "Add with Carry, Y-indexed, Indirect", &cpu_ADC, &cpu_IZY, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"ADC", "", &cpu_ADC, &cpu_ZPX, 4 },	{"ROR", "", &cpu_ROR, &cpu_ZPX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },	{"SEI", "", &cpu_SEI, &cpu_IMP, 2 },	{"ADC", "", &cpu_ADC, &cpu_ABY, 4 },	{"???", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"ADC", "", &cpu_ADC, &cpu_ABX, 4 },	{"ROR", "", &cpu_ROR, &cpu_ABX, 7 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },
		{"???", "Illegal Implied", &cpu_NOP, &cpu_IMP, 2 },	                        {"STA", "Store A, X-indexed, Indirect", &cpu_STA, &cpu_IZX, 6 },	{"???", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },	{"STY", "", &cpu_STY, &cpu_ZP0, 3 },	{"STA", "", &cpu_STA, &cpu_ZP0, 3 },	{"STX", "", &cpu_STX, &cpu_ZP0, 3 },	{"???", "", &cpu_XXX, &cpu_IMP, 3 },	{"DEY", "", &cpu_DEY, &cpu_IMP, 2 },	{"???", "", &cpu_NOP, &cpu_IMP, 2 },	{"TXA", "", &cpu_TXA, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"STY", "", &cpu_STY, &cpu_ABS, 4 },	{"STA", "", &cpu_STA, &cpu_ABS, 4 },	{"STX", "", &cpu_STX, &cpu_ABS, 4 },	{"???", "", &cpu_XXX, &cpu_IMP, 4 },
		{"BCC", "Branch on Carry Clear, Relative", &cpu_BCC, &cpu_REL, 2 },	        {"STA", "Store A, Y-indexed, Indirect", &cpu_STA, &cpu_IZY, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },	{"STY", "", &cpu_STY, &cpu_ZPX, 4 },	{"STA", "", &cpu_STA, &cpu_ZPX, 4 },	{"STX", "", &cpu_STX, &cpu_ZPY, 4 },	{"???", "", &cpu_XXX, &cpu_IMP, 4 },	{"TYA", "", &cpu_TYA, &cpu_IMP, 2 },	{"STA", "", &cpu_STA, &cpu_ABY, 5 },	{"TXS", "", &cpu_TXS, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },	{"???", "", &cpu_NOP, &cpu_IMP, 5 },	{"STA", "", &cpu_STA, &cpu_ABX, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },
		{"LDY", "Load Y, Immediate", &cpu_LDY, &cpu_IMM, 2 },	                    {"LDA", "Load A, X-indexed, Indirect", &cpu_LDA, &cpu_IZX, 6 },	{"LDX", "", &cpu_LDX, &cpu_IMM, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },	{"LDY", "", &cpu_LDY, &cpu_ZP0, 3 },	{"LDA",  "Load A Register", &cpu_LDA, &cpu_ZP0, 3 },	{"LDX", "", &cpu_LDX, &cpu_ZP0, 3 },	{"???", "", &cpu_XXX, &cpu_IMP, 3 },	{"TAY", "", &cpu_TAY, &cpu_IMP, 2 },	{"LDA",  "Load A Register", &cpu_LDA, &cpu_IMM, 2 },	{"TAX", "", &cpu_TAX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"LDY", "", &cpu_LDY, &cpu_ABS, 4 },	{"LDA",  "Load A Register", &cpu_LDA, &cpu_ABS, 4 },	{"LDX", "", &cpu_LDX, &cpu_ABS, 4 },	{"???", "", &cpu_XXX, &cpu_IMP, 4 },
		{"BCS", "Branch on Carry Set, Relative", &cpu_BCS, &cpu_REL, 2 },	        {"LDA", "Load A, Y-indexed, Indirect", &cpu_LDA, &cpu_IZY, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },	{"LDY", "", &cpu_LDY, &cpu_ZPX, 4 },	{"LDA",  "Load A Register", &cpu_LDA, &cpu_ZPX, 4 },	{"LDX", "", &cpu_LDX, &cpu_ZPY, 4 },	{"???", "", &cpu_XXX, &cpu_IMP, 4 },	{"CLV", "", &cpu_CLV, &cpu_IMP, 2 },	{"LDA",  "Load A Register", &cpu_LDA, &cpu_ABY, 4 },	{"TSX", "", &cpu_TSX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 4 },	{"LDY", "", &cpu_LDY, &cpu_ABX, 4 },	{"LDA",  "Load A Register", &cpu_LDA, &cpu_ABX, 4 },	{"LDX", "", &cpu_LDX, &cpu_ABY, 4 },	{"???", "", &cpu_XXX, &cpu_IMP, 4 },
		{"CPY", "Compare with Y, Immediate", &cpu_CPY, &cpu_IMM, 2 },	            {"CMP", "Compare with A, X-indexed, Indirect", &cpu_CMP, &cpu_IZX, 6 },	{"???", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"CPY", "", &cpu_CPY, &cpu_ZP0, 3 },	{"CMP", "", &cpu_CMP, &cpu_ZP0, 3 },	{"DEC", "", &cpu_DEC, &cpu_ZP0, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },	{"INY", "", &cpu_INY, &cpu_IMP, 2 },	{"CMP", "", &cpu_CMP, &cpu_IMM, 2 },	{"DEX", "", &cpu_DEX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"CPY", "", &cpu_CPY, &cpu_ABS, 4 },	{"CMP", "", &cpu_CMP, &cpu_ABS, 4 },	{"DEC", "", &cpu_DEC, &cpu_ABS, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },
		{"BNE", "Branch on Not Equal, Relative", &cpu_BNE, &cpu_REL, 2 },	        {"CMP", "Compare with A, Y-indexed, Indirect", &cpu_CMP, &cpu_IZY, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"CMP", "", &cpu_CMP, &cpu_ZPX, 4 },	{"DEC", "", &cpu_DEC, &cpu_ZPX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },	{"CLD", "", &cpu_CLD, &cpu_IMP, 2 },	{"CMP", "", &cpu_CMP, &cpu_ABY, 4 },	{"NOP", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"CMP", "", &cpu_CMP, &cpu_ABX, 4 },	{"DEC", "", &cpu_DEC, &cpu_ABX, 7 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },
		{"CPX", "Compare with X, Immediate", &cpu_CPX, &cpu_IMM, 2 },	            {"SBC", "", &cpu_SBC, &cpu_IZX, 6 },	{"???", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"CPX", "", &cpu_CPX, &cpu_ZP0, 3 },	{"SBC", "", &cpu_SBC, &cpu_ZP0, 3 },	{"INC", "", &cpu_INC, &cpu_ZP0, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 5 },	{"INX", "", &cpu_INX, &cpu_IMP, 2 },	{"SBC", "", &cpu_SBC, &cpu_IMM, 2 },	{"NOP", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_SBC, &cpu_IMP, 2 },	{"CPX", "", &cpu_CPX, &cpu_ABS, 4 },	{"SBC", "", &cpu_SBC, &cpu_ABS, 4 },	{"INC", "", &cpu_INC, &cpu_ABS, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },
		{"BEQ", "Branch on Equal, Relative", &cpu_BEQ, &cpu_REL, 2 },            	{"SBC", "", &cpu_SBC, &cpu_IZY, 5 },	{"???", "", &cpu_XXX, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 8 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"SBC", "", &cpu_SBC, &cpu_ZPX, 4 },	{"INC", "", &cpu_INC, &cpu_ZPX, 6 },	{"???", "", &cpu_XXX, &cpu_IMP, 6 },	{"SED", "", &cpu_SED, &cpu_IMP, 2 },	{"SBC", "", &cpu_SBC, &cpu_ABY, 4 },	{"NOP", "", &cpu_NOP, &cpu_IMP, 2 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 },	{"???", "", &cpu_NOP, &cpu_IMP, 4 },	{"SBC", "", &cpu_SBC, &cpu_ABX, 4 },	{"INC", "", &cpu_INC, &cpu_ABX, 7 },	{"???", "", &cpu_XXX, &cpu_IMP, 7 }
	};

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
		
		void ram_load_program(){
		
			printf("%sRam program mounted.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
		
			// Load Program (assembled at https://www.masswerk.at/6502/assembler.html)
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
			uint8_t prog[] = {0xA2,0x0A,0x8E,0x00,0x00,0xA2,0x03,0x8E,0x01,0x00,0xAC,0x00,0x00,0xA9,0x00,0x18,0x6D,0x01,0x00,0x88,0xD0,0xFA,0x8D,0x02,0x00,0xEA,0xEA,0xEA };
			
			uint16_t nOffset = 0x8000;
			
			
			int i = 0;
			while (i < 28)
			{
				bus.ram.data[nOffset] = prog[i];
				nOffset++;
				i++;
			}
	
			// Set Reset Vector
			bus.ram.data[0xFFFC] = 0x00;
			bus.ram.data[0xFFFD] = 0x80;

		
		}
		
		void ram_draw(uint16_t nAddr, int nRows, int nColumns)
		{
			
			for (int row = 0; row < nRows; row++)
			{
				printf("%s%#04X:\t", ANSI_COLOR_MAGENTA, nAddr);
				for (int col = 0; col < nColumns; col++)
				{
					if(bus.ram.data[nAddr] == 0){
						printf(" 00");
					} else {
						printf(" %X",  bus.ram.data[nAddr]);
					}
					nAddr += 1;
				}
				printf("%s\n", ANSI_COLOR_RESET);
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
		
			printf("%sAdded Ram device to bus.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
			bus.ram = ram;
		
		}
	
	/**
	* Write to attached RAM through Bus
	* @return void
	*/
		void bus_write(uint16_t addr, uint8_t data)
		{
		
			bus.ram.data[addr] = data;
		
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
// CPU FUNCTIONS

	void cpu_draw()
	{
		printf("STATUS:");
		printf(" %sN%s", (cpu.status & N ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
		printf(" %sV%s", (cpu.status & V ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
		printf(" %s-%s", (cpu.status & U ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
		printf(" %sB%s", (cpu.status & B ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
		printf(" %sD%s", (cpu.status & D ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
		printf(" %sI%s", (cpu.status & I ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
		printf(" %sZ%s", (cpu.status & Z ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
		printf(" %sC%s", (cpu.status & C ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
		printf(" %sC%s", (cpu.status & C ? ANSI_COLOR_GREEN : ANSI_COLOR_RED), ANSI_COLOR_RESET);
		printf(
			"\n%sPC: %#X %sSP: %#X %sA: %#X [%d] %sX: %#X [%d] %sY: %#X [%d] ",
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
			cpu.Y);
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
			INSTRUCTION lookupHere = lookup(cpuOPCode);
			if (lookupHere.addrmode != &cpu_IMP){
				cpuFetched = cpu_read(cpuAddrAbs);
			}
			return cpuFetched;
		}
	
	/**
	* Returns the value of a specific bit of the cpu.status register
	* @return uint8_t
	 */
		uint8_t cpu_get_flag(FLAGS6502 f)
		{
			return ((cpu.status & f) > 0) ? 1 : 0;
		}
	
	/**
	* Sets or clears a specific bit of the cpu.status register
	* @return uint8_t
	 */
		void cpu_set_flag(FLAGS6502 f, bool v)
		{
		
			if (v){
				cpu.status |= f;
			} else {
				cpu.status &= ~f;
			}
		
		}
	
	/**
	* Read RAM through Cpu->Bus
	* @return uint8_t
	*/
		uint8_t cpu_read(uint16_t addr)
		{
			return bus_read(addr);
		}
	
	/**
	* Write to RAM through Cpu->Bus
	* @return void
	*/
		void cpu_write(uint16_t addr, uint8_t data)
		{
			bus_write(addr, data);
		}
		
	/**
	*
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
			
			if(cuCyclesInit){
				printf("%sExecuted in %d Cycles ", ANSI_COLOR_MAGENTA, cpuCycles);
				cuCyclesInit = false;
			}
			
			while(cpuCycles > 0){
				printf("%s%d ", ANSI_COLOR_BLUE, cpuCycles);
				cpuCycles--;
				cpuClockCount++;
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
		
			printf("%sReset CPU PC to reset vector.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
		
			// Get address to set program counter to
				cpuAddrAbs = 0xFFFC;
				uint16_t lo = cpu_read(cpuAddrAbs);
				cpuAddrAbs = 0xFFFD;
				uint16_t hi = cpu_read(cpuAddrAbs);
		
			// Set it
				cpu.PC = (hi << 8) | lo;
		
			// Reset internal registers
				cpu.A = 0;
				cpu.X = 0;
				cpu.Y = 0;
				cpu.SP = 0xFD;
				cpu.status = 0x00 | U;
		
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
						cpu.PC = (hi << 8) | lo;
			
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
			cpu.PC = (hi << 8) | lo;
		
			cpuCycles = 8;
			cuCyclesInit = true;
		}
	
	/**
	* Perform one operations worth of emulation
	* @return void
	*/
		void cpu_execute()
		{
		
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
						cpuOPCode = cpu_read(cpu.PC);
						
					//get opcode struct and display info about this operation
						cpuIns = lookup(cpuOPCode);
						printf("%sInstruction Opcode: %#X (%s) from memory: %#X / %d.%s\n", ANSI_COLOR_YELLOW,cpuOPCode, cpuIns.name, cpu.PC, cpu.PC,  ANSI_COLOR_RESET);
						
					// Increment program counter, we have now read the cpuOPCode byte
						cpu.PC++;
					
				//-------------------------------------------------------------------------------
	
			// Get Starting number of cycles from instruction struct
				cpuCycles = cpuIns.cycles;
				cuCyclesInit = true;
	
			// Perform fetch of intermmediate data using the
			// required addressing mode
				uint8_t additional_cycle1 = (*cpuIns.addrmode)();
	
			//1+ cycles
				//-------------------------------------------------------------------------------
				
					// Perform operation, calling the function defined in that element of the instruction struct array
						uint8_t additional_cycle2 = (*cpuIns.operate)();
						
				//-------------------------------------------------------------------------------
	
			// The addressmode and operation may have altered the number
			// of cycles this instruction requires before its completed
				cpuCycles += (additional_cycle1 & additional_cycle2);
	
			// Always set the unused status flag bit to 1
				cpu_set_flag(U, true);
				
			//output cycles for this operation
				cpu_run_cycles();
				
			if(cpu.PC == 0xFFFF){
				cpuExecuting = false;
			}
				
		}
	
	/**
	* Return INSTRUCTION struct from lookup struct array
	* @return INSTRUCTION
	*/
		INSTRUCTION lookup(uint8_t opCode)
		{
		
			return lookupM[opCode];
		
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
	* A full 16-bit address is loaded and used
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
				cpu.A = cpuTemp & 0x00FF;
			
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
				cpuTemp = (uint16_t)cpu.A + value + (uint16_t)cpu_get_flag(C);
				cpu_set_flag(C, cpuTemp & 0xFF00);
				cpu_set_flag(Z, ((cpuTemp & 0x00FF) == 0));
				cpu_set_flag(V, (cpuTemp ^ (uint16_t)cpu.A) & (cpuTemp ^ value) & 0x0080);
				cpu_set_flag(N, cpuTemp & 0x0080);
				cpu.A = cpuTemp & 0x00FF;
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
			cpu.A = cpu.A & cpuFetched;
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
			if (lookup(cpuOPCode).addrmode == &cpu_IMP)
				cpu.A = cpuTemp & 0x00FF;
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
				
				cpu.PC = cpuAddrAbs;
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
		
				cpu.PC = cpuAddrAbs;
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
		
				cpu.PC = cpuAddrAbs;
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
		
				cpu.PC = cpuAddrAbs;
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
		
				cpu.PC = cpuAddrAbs;
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
		
				cpu.PC = cpuAddrAbs;
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
		
			cpu.PC = cpu_read(0xFFFE) | (cpu_read(0xFFFF) << 8);
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
		
				cpu.PC = cpuAddrAbs;
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
		
				cpu.PC = cpuAddrAbs;
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
			cpu.A = cpu.A ^ cpuFetched;
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
			cpu.PC = cpuAddrAbs;
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
		
			cpu.PC = cpuAddrAbs;
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
			cpu.A = cpuFetched;
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
			cpu_fetch();
			cpu.X = cpuFetched;
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
			cpu_fetch();
			cpu.Y = cpuFetched;
			cpu_set_flag(Z, cpu.Y == 0x00);
			cpu_set_flag(N, cpu.Y & 0x80);
			return 1;
		}
	
	/**
	* @return uint8_t
	*/
		uint8_t cpu_LSR(){
			cpu_fetch();
			cpu_set_flag(C, cpuFetched & 0x0001);
			cpuTemp = cpuFetched >> 1;
			cpu_set_flag(Z, (cpuTemp & 0x00FF) == 0x0000);
			cpu_set_flag(N, cpuTemp & 0x0080);
			if (lookup(cpuOPCode).addrmode == &cpu_IMP)
				cpu.A = cpuTemp & 0x00FF;
			else
				cpu_write(cpuAddrAbs, cpuTemp & 0x00FF);
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
			cpu_fetch();
			cpu.A = cpu.A | cpuFetched;
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
			cpu.A = cpu_read(0x0100 + cpu.SP);
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
			if (lookup(cpuOPCode).addrmode == &cpu_IMP)
				cpu.A = cpuTemp & 0x00FF;
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
			if (lookup(cpuOPCode).addrmode == &cpu_IMP)
				cpu.A = cpuTemp & 0x00FF;
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
			cpu.PC = cpu_read(0x0100 + cpu.SP);
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
			cpu.PC = cpu_read(0x0100 + cpu.SP);
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
			return 0;
		}
	
	/**
	* Instruction: Set Decimal Flag
	* Function:    D = 1
	* * @return uint8_t
	*/
		uint8_t cpu_SED(){
			cpu_set_flag(D, true);
			return 0;
		}
	
	/**
	* Instruction: Set Interrupt Flag / Enable Interrupts
	* Function:    I = 1
	* * @return uint8_t
	*/
		uint8_t cpu_SEI(){
			cpu_set_flag(I, true);
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
			cpu.X = cpu.A;
			cpu_set_flag(Z, cpu.X == 0x00);
			cpu_set_flag(N, cpu.X & 0x80);
			return 0;
		}
	
	/**
	* Instruction: Transfer Accumulator to Y Register
	* Function:    Y = A
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_TAY(){
			cpu.Y = cpu.A;
			cpu_set_flag(Z, cpu.Y == 0x00);
			cpu_set_flag(N, cpu.Y & 0x80);
			return 0;
		}
	
	/**
	* Instruction: Transfer Stack Pointer to X Register
	* Function:    X = stack pointer
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_TSX(){
			cpu.X = cpu.SP;
			cpu_set_flag(Z, cpu.X == 0x00);
			cpu_set_flag(N, cpu.X & 0x80);
			return 0;
		}
	
	/**
	* Instruction: Transfer X Register to Accumulator
	* Function:    A = X
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_TXA(){
			cpu.A = cpu.X;
			cpu_set_flag(Z, cpu.A == 0x00);
			cpu_set_flag(N, cpu.A & 0x80);
			return 0;
		}
	
	/**
	* Instruction: Transfer X Register to Stack Pointer
	* Function:    stack pointer = X
	* * @return uint8_t
	*/
		uint8_t cpu_TXS(){
			cpu.SP = cpu.X;
			return 0;
		}
		
	/**
	* Instruction: Transfer Y Register to Accumulator
	* Function:    A = Y
	* Flags Out:   N, Z
	* * @return uint8_t
	*/
		uint8_t cpu_TYA(){
			cpu.A = cpu.Y;
			cpu_set_flag(Z, cpu.A == 0x00);
			cpu_set_flag(N, cpu.A & 0x80);
			return 0;
		}
	
	/**
	* This function captures illegal cpuOPCodes
	* @return uint8_t
	*/
		uint8_t cpu_XXX(){
			return 0;
		}


int main(int argc, char *argv[])
{

	// If the first character of optstring is '-', then each nonoption argv-element is handled as if
	// it were the argument of an option with character code 1.
	// (This is used by programs that were written to expect options and other argv-elements in any order and that care about the ordering of the two.)
		bool outputAllOnExecute = false;
		int opt;
		while ((opt = getopt(argc, argv, "-a")) != -1) {
			
			switch (opt) {
				
				//get file pointer
					case 'a':
						outputAllOnExecute = true;
					break;
					
				//option not in optstring
					case '?':
					default:
						printf("Option not in option list of [a]");
						exit(EXIT_FAILURE);
				
			}
			
		}

	printf("\n%sPort and modification from OLC 6502 C++ emulator.%s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
	
	//add ram to bus
		bus_add_devices();
	
	//clear ram of any standing data
		ram_clear();
	
	//load program into ram
		ram_load_program();
	
	//turn on the system
		cpu_reset();
	
	printf("\n%s6502 in C Powered On.%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
	printf("\n%sPress Enter to step through each instruction.%s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
	
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
						if(outputAllOnExecute && strcmp(cpuIns.name, "NOP") != 0){
							printf("\n");
							ram_draw(0x0000, 16, 16);
							printf("\n");
							ram_draw(0x8000, 16, 16);
							printf("\n");
							cpu_draw();
							printf("\n");
						}
						
					}
				}
			
			//memory dump
				else if(strcmp(&consoleCommand[0], "m") == 0){
					printf("Memory Dump\n");
					ram_draw(0x0000, 16, 16);
					printf("\n");
					ram_draw(0x8000, 16, 16);
					ignoreNextEnter = true;
				}
				
			//cpu dump
				else if(strcmp(&consoleCommand[0], "c") == 0){
					printf("CPU Dump\n");
					cpu_draw();
					printf("\n");
					ignoreNextEnter = true;
				}
				
				
			//stack dump
				else if(strcmp(&consoleCommand[0], "s") == 0){
					printf("Stack Dump\n");
					ignoreNextEnter = true;
				}
				
			//all
				else if(strcmp(&consoleCommand[0], "a") == 0){
					printf("System Dump\n");
					ram_draw(0x0000, 16, 16);
					printf("\n");
					ram_draw(0x8000, 16, 16);
					printf("\n");
					cpu_draw();
					printf("\n");
					ignoreNextEnter = true;
				}
				
			//any other char bleeds round the enter key at the end of stdin so cpu_execute will fire
			
		}
		
	}
	
	return 0;
	
}
