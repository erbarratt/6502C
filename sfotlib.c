#include "sfotlib.h"

//initialise global vars
	struct Mem mem;
	struct CPU cpu;

	u32 cycles = 0;

//list of instructions


/**
* Reset CPU Program Counter, Tack Pointer and Registers
* @return void
*/
	void ResetCpu(){
	
		printOut("CPU Initialised", "", CHAR, "yellow");
	
		cpu.PC = (Word)0xFFFC;
		//cpu.PC = (Word)0;
		cpu.SP = (Byte)0x0100;
		cpu.D = (Byte)0;
		cpu.A = (Byte)0;
		cpu.X = (Byte)0;
		cpu.Y = (Byte)0;
	
	}

/**
* Clear memory
* @return void
*/
	void InitialiseMem(){
	
		printOut("Memory Initialised", "", CHAR, "yellow");
	
		u32 i;
		for(i = 0; i < MAX_MEM; i++){
			mem.Data[i] = 0;
		}
	
	}

/**
* Fetch one byte of memory at location of CPU Program Counter
* Decrement cycles
* @return Byte
*/
	Byte FetchByte(char * msg){
		
		Byte Data = mem.Data[cpu.PC];
		cpu.PC++;
		cycles--;
		printOut(msg, &Data, BYTE, "blue");
		return Data;
		
	}

/**
* Fetch one byte of memory at location given
* @return Byte
*/
	Byte ReadByte(Byte addr, char * msg){
		
		Byte Data = mem.Data[addr];
		cycles--;
		printOut(msg, &Data, BYTE, "cyan");
		return Data;
		
	}
	
/**
* Set common flags for LDA instructions
* @return void
*/
	void LDASetStatus(){
	
		printOut("LDA Status Z and N set.", "", CHAR, "yellow");
	
		//zero flag off
			cpu.Z = (cpu.A == 0);
			
		//Set if bit 7 of A is set
			cpu.N = ((cpu.A & (1<<7)) > 0);
	
	}

/**
* Execute a given amount of cycles, and process instruction
* @param u32 cycleAm How many cycles to execute
* @return void
*/
	void Execute(u32 cycleAm){
	
		cycles = cycleAm;
		
		printOut("Executing %d cycles.", &cycles, BYTE, "yellow");
		
		while(cycles > 0){
		
			//fetch byte instruction
				Byte Ins = FetchByte("");
				
			switch(Ins){
			
				//LDA - Load Accumulator
				//Loads a byte of memory into the accumulator setting the zero and negative flags as appropriate.
				//Immediate
					case INS_LDA_IMD:
					{
					
						printOut("%#X LDA Zero Page Instruction received", &Ins, BYTE, "magenta");
					
						//set value
							cpu.A = FetchByte("%#x Byte Fetched");
							
						LDASetStatus();
					
					}
					break;
					
				//Zero Page
					case INS_LDA_ZPG:
					{
					
						printOut("%#X LDA Zero Page Instruction received", &Ins, BYTE, "magenta");
					
						//get address
							Byte zeroPageAddr = FetchByte("%#x Byte Fetched");
							
						//set value at address into zero flag
							cpu.A = ReadByte(zeroPageAddr, "%#x Byte Read");
							
						LDASetStatus();
					
					}
					break;
					
				//Zero Page, X
					case INS_LDA_ZPX:
					{
					
						printOut("%#X LDA Zero Page, X Instruction received", &Ins, BYTE, "magenta");
					
						//get address
							Byte zeroPageAddr = FetchByte("%#x Byte Fetched");
							
						zeroPageAddr = (Byte)(zeroPageAddr + cpu.X);
						
						cycles--;
						
						//set value at address into zero flag
							cpu.A = ReadByte(zeroPageAddr, "%#x Byte Read");
							
						LDASetStatus();
					
					}
					break;
					
				//instruction unknown
					default:
						printOutErr("Processor instruction not handled: %u", &Ins, BYTE);
					break;
					
			}
			
		}
		
		
		
	}
