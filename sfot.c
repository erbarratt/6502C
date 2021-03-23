#include "sfotlib.h"

int main(){

	ResetCpu();
	InitialiseMem();
	Execute(2);
	
	printf("%d\n", cpu.A);
	
	return 0;
 
}