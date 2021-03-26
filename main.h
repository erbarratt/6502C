#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint-gcc.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define MAX_MEM (1024 * 64)

	char ANSI_COLOR_RED[] = "\x1b[31m";
	char ANSI_COLOR_GREEN[] = "\x1b[32m";
	char ANSI_COLOR_YELLOW[] = "\x1b[33m";
	char ANSI_COLOR_BLUE[] = "\x1b[34m";
	char ANSI_COLOR_MAGENTA[] = "\x1b[35m";
	char ANSI_COLOR_CYAN[] = "\x1b[36m";
	char ANSI_COLOR_RESET[] = "\x1b[0m";

typedef struct {

	uint8_t data[MAX_MEM];

} RAM;

typedef struct {

	RAM ram;

} Bus;

typedef enum
{

	C = (1 << 0),	// Carry Bit
	Z = (1 << 1),	// Zero
	I = (1 << 2),	// Disable Interrupts
	D = (1 << 3),	// Decimal Mode (unused in this implementation)
	B = (1 << 4),	// Break
	U = (1 << 5),	// Unused
	V = (1 << 6),	// Overflow
	N = (1 << 7),	// Negative
	
} FLAGS6502;

typedef struct
{

	//Program Counter
		uint16_t PC;
		
	//Stack Pointer
		uint8_t SP;
	
	//Accumulator
		uint8_t A;
		
	//Index Register X
		uint8_t X;
		
	//Index Register Y
		uint8_t Y;
	
	//processor status
		FLAGS6502 status;

} CPU;

// This structure and the following vector are used to compile and store
// the opcode translation table. The 6502 can effectively have 256
// different instructions. Each of these are stored in a table in numerical
// order so they can be looked up easily, with no decoding required.
// Each table entry holds:
//	Pneumonic : A textual representation of the instruction (used for disassembly)
//	Opcode Function: A function pointer to the implementation of the opcode
//	Opcode Address Mode : A function pointer to the implementation of the
//						  addressing mechanism used by the instruction
//	Cycle Count : An integer that represents the base number of clock cycles the
//				  CPU requires to perform the instruction
	typedef struct
	{
		char name[50];
		char label[50];
		uint8_t     (*operate )(void);
		uint8_t     (*addrmode)(void);
		uint8_t cycles;
	} INSTRUCTION;

// Addressing Modes =============================================
// The 6502 has a variety of addressing modes to access data in
// memory, some of which are direct and some are indirect (like
// pointers in C++). Each opcode contains information about which
// addressing mode should be employed to facilitate the
// instruction, in regards to where it reads/writes the data it
// uses. The address mode changes the number of bytes that
// makes up the full instruction, so we implement addressing
// before executing the instruction, to make sure the program
// counter is at the correct location, the instruction is
// primed with the addresses it needs, and the number of clock
// cycles the instruction requires is calculated. These functions
// may adjust the number of cycles required depending upon where
// and how the memory is accessed, so they return the required
// adjustment.

	uint8_t cpu_IMP();	uint8_t cpu_IMM();
	uint8_t cpu_ZP0();	uint8_t cpu_ZPX();
	uint8_t cpu_ZPY();	uint8_t cpu_REL();
	uint8_t cpu_ABS();	uint8_t cpu_ABX();
	uint8_t cpu_ABY();	uint8_t cpu_IND();
	uint8_t cpu_IZX();	uint8_t cpu_IZY();
	
// Opcodes ======================================================
// There are 56 "legitimate" opcodes provided by the 6502 CPU. I
// have not modelled "unofficial" opcodes. As each opcode is
// defined by 1 byte, there are potentially 256 possible codes.
// Codes are not used in a "switch case" style on a processor,
// instead they are repsonisble for switching individual parts of
// CPU circuits on and off. The opcodes listed here are official,
// meaning that the functionality of the chip when provided with
// these codes is as the developers intended it to be. Unofficial
// codes will of course also influence the CPU circuitry in
// interesting ways, and can be exploited to gain additional
// functionality!
//
// These functions return 0 normally, but some are capable of
// requiring more clock cycles when executed under certain
// conditions combined with certain addressing modes. If that is
// the case, they return 1.
//
// I have included detailed explanations of each function in
// the class implementation file. Note they are listed in
// alphabetical order here for ease of finding.

	uint8_t cpu_ADC();	uint8_t cpu_AND();	uint8_t cpu_ASL();	uint8_t cpu_BCC();
	uint8_t cpu_BCS();	uint8_t cpu_BEQ();	uint8_t cpu_BIT();	uint8_t cpu_BMI();
	uint8_t cpu_BNE();	uint8_t cpu_BPL();	uint8_t cpu_BRK();	uint8_t cpu_BVC();
	uint8_t cpu_BVS();	uint8_t cpu_CLC();	uint8_t cpu_CLD();	uint8_t cpu_CLI();
	uint8_t cpu_CLV();	uint8_t cpu_CMP();	uint8_t cpu_CPX();	uint8_t cpu_CPY();
	uint8_t cpu_DEC();	uint8_t cpu_DEX();	uint8_t cpu_DEY();	uint8_t cpu_EOR();
	uint8_t cpu_INC();	uint8_t cpu_INX();	uint8_t cpu_INY();	uint8_t cpu_JMP();
	uint8_t cpu_JSR();	uint8_t cpu_LDA();	uint8_t cpu_LDX();	uint8_t cpu_LDY();
	uint8_t cpu_LSR();	uint8_t cpu_NOP();	uint8_t cpu_ORA();	uint8_t cpu_PHA();
	uint8_t cpu_PHP();	uint8_t cpu_PLA();	uint8_t cpu_PLP();	uint8_t cpu_ROL();
	uint8_t cpu_ROR();	uint8_t cpu_RTI();	uint8_t cpu_RTS();	uint8_t cpu_SBC();
	uint8_t cpu_SEC();	uint8_t cpu_SED();	uint8_t cpu_SEI();	uint8_t cpu_STA();
	uint8_t cpu_STX();	uint8_t cpu_STY();	uint8_t cpu_TAX();	uint8_t cpu_TAY();
	uint8_t cpu_TSX();	uint8_t cpu_TXA();	uint8_t cpu_TXS();	uint8_t cpu_TYA();

// I capture all "unofficial" opcodes with this function. It is
// functionally identical to a NOP
	uint8_t cpu_XXX();
