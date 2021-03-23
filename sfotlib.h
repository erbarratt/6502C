#include "sfotcommon.h"

#define MAX_MEM 1024 * 64

//Load Accumulator
	#define INS_LDA_IMD 0xA9         //Immediate
	#define INS_LDA_ZPG 0xA5         //Zero Page
	#define INS_LDA_ZPX 0xB5         //Zero Page, X
	#define INS_LDA_ABS 0xAD         //Absolute
	#define INS_LDA_ABX 0xBD         //Absolute, X
	#define INS_LDA_ABY 0xB9         //Absolute, Y

extern struct Mem{
	Byte Data[MAX_MEM];
} mem;

extern struct CPU{
	
	Word PC;            //program counter
	Byte SP;            //stack pointer
	
	//Accumulator
		Byte A;
	//Index Register X
		Byte X;
	//Index Register Y
		Byte Y;
	
	//processor status
		Byte C : 1;     //Carry Flag
		Byte Z : 1;     //Zero Flag
		Byte I : 1;     //Interrupt Disable
		Byte D : 1;     //Decimal Mode
		Byte B : 1;     //Break Command
		Byte V : 1;     //Overflow Flag
		Byte N : 1;     //Negative Flag
	
} cpu;

extern u32 cycles;

void ResetCpu();
void InitialiseMem();
Byte FetchByte(char * msg);
Byte ReadByte(Byte addr, char * msg);
void LDASetStatus();
void Execute(u32 cycleAm);
