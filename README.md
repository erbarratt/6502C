# 6502C
6502 Emulator in C

Original code in C++ written by onelonecoder / onelonecoder.com
	
https://github.com/OneLoneCoder/olcNES/tree/master/Part%232%20-%20CPU
	
Find David on youtube: https://www.youtube.com/watch?v=8XmxKPJDGU0
	
His tutorials have inspired me to dive lower down in languages.
	
This C source is a nearly direct port of his code, moving from OOP to functional
and adding some helper functions + a pseudo display device to hook into the bus.
	
All cpu_&lt;OPCODE&gt; + cpu_&lt;ADDRESSMODE&gt; functions are basically unmodified, as well as the lookup array of INSTRUCTION structs.

#Compile
Download the repository and then use

<pre>sudo make makefile.dev</pre>

Use ./sfot to launch

<pre>sudo ./sfot</pre>

# Options

This program will accept:

-a Always output all items

-m Always output Memory dump

-c Always output CPU dump

-p Always output program lines

-d Always output display device dump

<pre>./sfot -a</pre>

# Usage
Press enter to walk through CPU instructions. Depending on options tags, various additional elements will be output to the console.

Optionally enter a, m, c, p, d then enter to get the corresponding output element.

# Programs
Use https://github.com/erbarratt/txtchartobin to convert txt files of hex numbers into a binary files that this program can read and use. 
If post.bin does not exist in the same directory as this file, a default Hello World program will load into the emulator memory.
