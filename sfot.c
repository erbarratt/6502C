#include "sfotlib.h"

int main(){

	ResetCpu();
	InitialiseMem();
	
	//---start inline
	
		mem.Data[0xFFFC] = INS_LDA_ZPX;
		mem.Data[0xFFFD] = 0x42;
		mem.Data[0x42] = 0x84;
		
		mem.Data[0xFFFE] = INS_LDA_IMD;
		mem.Data[0xFFFF] = 0x42;
	
	//---end inline
	
	Execute(6);
	
	printf("%#x\n", cpu.A);
	
	return 0;
 
}