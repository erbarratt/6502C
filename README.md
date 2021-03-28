# 6502C
6502 Emulator in C

#Compile
Download the repository and then use

<pre>sudo make makefile.dev</pre>

Use ./sfot to launch

<pre>sudo ./sfot</pre>

#Options
This program will accept:

-a Always output all items

-m Always output Memory dump

-c Always output CPU dump

-p Always output program lines

-d Always output display device dump

<pre>./sfot -a</pre>

#Usage
Press enter to walk through CPU instructions. Depending on options tags, various additional elements will be output to the console.

Optionally enter a, m, c, p, d then enter to get the corresponding output element.

#Programs
Use https://github.com/erbarratt/txtchartobin to convert txt files of hex numbers into a binary files that this program can read and use. 
If post.bin does not exist in the same directory as this file, a default Hello World program will load into the emulator memory.