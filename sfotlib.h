#include "sfotcommon.h"

typedef unsigned char Byte;
typedef unsigned short Word;
typedef unsigned int u32;

#define MAX_MEM 1024 * 64

struct Mem{
	Byte Data[MAX_MEM];
};

struct CPU{
	
	Word PC;        //program counter
	Byte SP;        //stack pointer
	
	Byte A;
	Byte X;
	Byte Y;     //registers
	
	Byte C : 1;
	Byte Z : 1;
	Byte I : 1;
	Byte D : 1;
	Byte B : 1;
	Byte V : 1;
	Byte N : 1;
	
};

extern struct Mem mem;
extern struct CPU cpu;
extern u32 cycles;

void ResetCpu();
void InitialiseMem();
Byte FetchByte();
void Execute(u32 cycleAm);
