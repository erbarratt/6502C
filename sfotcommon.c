#include "sfotcommon.h"

char ANSI_COLOR_RED[] = "\x1b[31m";
char ANSI_COLOR_GREEN[] = "\x1b[32m";
char ANSI_COLOR_YELLOW[] = "\x1b[33m";
char ANSI_COLOR_BLUE[] = "\x1b[34m";
char ANSI_COLOR_MAGENTA[] = "\x1b[35m";
char ANSI_COLOR_CYAN[] = "\x1b[36m";
char ANSI_COLOR_RESET[] = "\x1b[0m";

/**
* Set the colour output of the console
* @param char *col The chosen colour
* @return void
*/
	void colorSet(char *col){
		
		if (strcmp(col, "red") == 0){
			
			printf("%s", ANSI_COLOR_RED);
			
		} else if (strcmp(col, "yellow") == 0){
			
			printf("%s", ANSI_COLOR_YELLOW);
			
		} else if (strcmp(col, "green") == 0){
			
			printf("%s", ANSI_COLOR_GREEN);
			
		} else if (strcmp(col, "blue") == 0){
			
			printf("%s", ANSI_COLOR_BLUE);
			
		} else if (strcmp(col, "cyan") == 0){
			
			printf("%s", ANSI_COLOR_CYAN);
			
		} else if (strcmp(col, "magenta") == 0){
			
			printf("%s", ANSI_COLOR_MAGENTA);
			
		} else {
			
			printf("%s", ANSI_COLOR_RESET);
			
		}
		
	}