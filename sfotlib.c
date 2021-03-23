#include "sfotlib.h"

struct Mem mem;
struct CPU cpu;
u32 cycles = 0;

const Byte INS_LDA_IM = 0xA9;

void ResetCpu(){

	cpu.PC = (Word)0xFFFC;
	cpu.SP = (Byte)0x0100;
	cpu.D = 0;
	cpu.A = 0;
	cpu.X = 0;
	cpu.Y = 0;

}

void InitialiseMem(){

	u32 i;
	for(i = 0; i < MAX_MEM; i++){
		mem.Data[i] = 0;
	}

}

Byte FetchByte(){
	
	Byte Data = mem.Data[cpu.PC];
	cpu.PC++;
	cycles--;
	return Data;
}

void Execute(u32 cycleAm){

	cycles = cycleAm;
	
	while(cycles > 0){
	
		//fetch byte instruction
			Byte Ins = FetchByte();
		
		//LDA - Load Accumulator
		//Loads a byte of memory into the accumulator setting the zero and negative flags as appropriate.
			if(Ins == INS_LDA_IM){
			
				//set value
					cpu.A = FetchByte(cycles);
					
				//zero flag off
					cpu.Z = (cpu.A == 0);
					
				//Set if bit 7 of A is set
					cpu.N = ((cpu.A & (1<<7)) > 0);
					
			}
			
		//instruction not handled
			else {
			
			}
		
	}
	
	
	
}
