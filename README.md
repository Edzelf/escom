# escom
Communication for small Forth devices.
Like e4thcom, but it will run under Windows.

escom is a terminal program that runs under Windows.  It can be used for communicating with an embedded Forth system, like "stm8ef" for the STM8 family or the "mecrisp" for the STM32 family.  The connection between the Windows computer and the target Forth system is trhrough a USB to Serial converter.

escom is based on e4thcom by Manfred Mahlow.  The program was converted to C for Windows compatibility.

The source of escom (src/escom.c) can be compiled by the gcc compiler.  A Windows executable can be downloded from windows/escom.exe.

Short description:
For a start, a Forth console is needed to control the embedded system.  A simple program like “putty” is sufficient for communication.  It is also possible to add new words to the system.  But as soon as the first program is written and ready for testing, an easy way to upload the source code files is missing.  Simple line editing is also not possible with a standard communication program.  A tool to overcome these deficiencies is the escom terminal program.
In terminal mode it sends terminal input without a local echo via serial line to the target (Forth System) and displays characters received from the target in the terminal window.
The escom program can also be used to upload sourcecode to the target system.  This can be user written software or readily available library words.
As imbedded system often use symbolic names for the various I/O registers, escom has -just like e4thcom- the capability to make use of these symbolic name in the source code.

See the document in the "doc" directory for details.

21-12-2021, ES: Allow non standard baudrates.
15-05-2022, ES: Added zepto support, thanks to tabeman.
