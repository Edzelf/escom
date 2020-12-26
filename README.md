# escom
Communication for small Forth devices.
Like e4thcom, but it will run under Windows.

escom is a terminal program that runs under Windows.  It can be used for communicating with an embedded Forth system, like the stm8ef for the STM8 family.  The connection between the WIndows computer and the target Forth system is trhrough a USB to Serial converter.

escom is based on e4thcom by Manfred Mahlow.  The program was converted to C for Windows compatibility.	

For a start, the Forth console can be used to control the embedded system.  A simple program like “putty” is sufficient for communication.  It is also possible to add new words to the system.  But as soon as the first program is written and ready for testing, an easy way to upload the source code files is missing.  Simple line editing is also not possible with a standard communication program.  A tool to overcome these deficiencies is the escom terminal program.
In terminal mode it sends terminal input without a local echo via serial line to the target (Forth System) and displays characters received from the target in the terminal window.
The escom program can also be used to upload sourcecode to the target system.  This can be user written software or readily available library words.
As imbedded system often use symbolic names for the various I/O registers, escom has -just like e4thcom- the capability to make use of these symbolic name in the source code.

See the doc directory for details.
