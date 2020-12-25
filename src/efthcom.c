//***************************************************************************************************
//                              E 4 T H C O M . C						    *
//***************************************************************************************************
// e4thcom.c											    *
// Terminal program for embedded Forth Systems with interfaces that supports conditional and	    *
// unconditional uploading of source code.							    *
// Based on e4thcom by Manfred Mahlow.								    *
// The program was converted to C for Windows compatibility.					    *
// Tested on Debian Perl on Windows (WSL).                                              	    *
// Written by Ed Smallenburg.                                                                       *
//***************************************************************************************************
// e4thcom.pl reads lines from the teminal (with line editing).  Completed lines are forwarded to   *
// the target Forth device.                                                                         *
// Lines starting with "//" or "\" are not forwarded, but interpreted by this program.              *
// Configuration parameters are in e4thcom.conf.                                                    *
//***************************************************************************************************
//                                                                                                  *
// Revision    Auth.  Remarks									    *
// ----------  -----  ----------------------------------------------------------------------------- *
// 18-12-2020  ES     Version 1.0,  first set-up.						    *
//***************************************************************************************************
#include <stdio.h>	// Standard input/output definitions
#include <stdlib.h>	// Standard library definitions
#include <string.h>	// String function definitions
#include <unistd.h>	// UNIX standard function definitions
#include <fcntl.h>	// File control definitions
#include <errno.h>	// Error number definitions
#include <windows.h>

// Constants:
#define VERSION "1.0"					// The version number

// Global variables
char   device[32] = "COM5" ;				// Default serial port for target connection
int    baudrate = 115200 ;				// Default baudrate for communication
HANDLE hcom ;						// Handle for serial I/O
int    tokc ;						// Number of tokens in tokv
char*  tokv[32] ;					// Tokens in config file


//***************************************************************************************************
//			T O K E N I Z E _ C O N F _ F I L E					    *
//***************************************************************************************************
// Parse the options in the config file.							    *
//***************************************************************************************************
void tokenize_conf_file()
{
  char        filename[128] ;				// Path of config file
  char        line[128] ;				// Input buffer for 1 line
  FILE*       fp ;					// File handle

  GetModuleFileName ( NULL, filename,			// Get path of executable
                      sizeof(filename) ) ;
  printf ( "Exe name is %s\n", filename ) ;
#ifdef bla
  fp = fopen ( filename, "r" ) ;			// Open the file
  if ( fp == NULL)					// Success?
  {
    printf ( "Unable to open %s!", filename ) ;		// No, show error
    return FALSE ;
  }
  print_sep() ;						// Print separation line
  while ( fgets ( line, sizeof(line), fp ) != NULL )
  {
    printf ( "%s", line ) ;
  }
  fclose ( fp ) ;					// Close input file
  printf ( "\n" ) ;					// Extra newline
  print_sep() ;						// Print separation line
#endif
}


//***************************************************************************************************
//				P A R S E _ O P T I O N						    *
//***************************************************************************************************
// Parse the commandline options.								    *
//***************************************************************************************************
void parse_options ( int argc, char* argv[] )
{
  int optchar ;
 
  while ( ( optchar = getopt ( argc, argv, "d:b:" ) ) != -1 )	// Get next option
    switch ( optchar )
    {
      case 'd' :						// Device spec?
        strncpy ( device, optarg, sizeof(device) - 1 ) ;	// Yes set device
        break;
      case 'b':							// Baudrate?
        baudrate = atoi ( optarg ) ;				// Yes, set baudrate		
        break;
    }
}


//***************************************************************************************************
//				C O M _ T I M E O U T						    *
//***************************************************************************************************
// Set the time-out for the com port.								    *
//***************************************************************************************************
void com_timeout ( int t )
{
  COMMTIMEOUTS timeouts = { 0 } ;			// Struct for setting constants

  GetCommTimeouts ( hcom, &timeouts ) ;			// Read current time-out settings
  // For input:
  timeouts.ReadIntervalTimeout         = 5 ;		// in milliseconds
  timeouts.ReadTotalTimeoutConstant    = t ;		// in milliseconds
  timeouts.ReadTotalTimeoutMultiplier  = 0 ;		// in milliseconds
  // For output:
  timeouts.WriteTotalTimeoutConstant   = 0 ;		// in milliseconds
  timeouts.WriteTotalTimeoutMultiplier = 0 ;		// in milliseconds
  SetCommTimeouts ( hcom, &timeouts ) ;			// Set time-outs
}


//***************************************************************************************************
//					O P E N _ P O R T					    * 
//***************************************************************************************************
// Open serial port to target device.								    *
// Returns -1 on error.										    *
//***************************************************************************************************
BOOL open_port ( const char* port )
{
  char wport[32] ;					// Port name in windows form
  BOOL res ;						// Result of opening
  DCB dcbParams = { 0 } ;				// Initializing DCB structure

  sprintf ( wport, "\\\\.\\%s", port ) ;		// Format for Windows
  hcom = CreateFile ( wport,		                // port name
                      GENERIC_READ | GENERIC_WRITE,	// Read/Write
                      0,				// No Sharing
                      NULL,				// No Security
                      OPEN_EXISTING,			// Open existing port only
                      0,				// Non Overlapped I/O
                      NULL ) ;				// Null for Comm Devices

  if ( hcom == INVALID_HANDLE_VALUE )			// Check result
  {
    printf ( "Error in opening %s", port ) ;		// No success
    return FALSE ;
  }
  dcbParams.DCBlength = sizeof(dcbParams) ;		// Set size
  GetCommState ( hcom, &dcbParams ) ;			// Get current state
  dcbParams.BaudRate = baudrate ;			// Setting BaudRate = 9600
  dcbParams.ByteSize = 8 ;				// Setting ByteSize = 8
  dcbParams.StopBits = ONESTOPBIT ;			// Setting StopBits = 1
  dcbParams.Parity   = NOPARITY ;			// Setting Parity = None
  SetCommState ( hcom, &dcbParams ) ;			// Set new status
  com_timeout ( 50 ) ;					// Set default time-out
  return TRUE ;						// Return positive result
}


//***************************************************************************************************
//				W R I T E C O M							    *
//***************************************************************************************************
// Write a buffer to the serial port.								    *
//***************************************************************************************************
BOOL writecom ( const char* buf )
{
  BOOL  stat ;						// Result of write action
  DWORD nbToWrite ;					// Number of bytes to write
  DWORD nbWritten ;					// Bytes written

  nbToWrite = strlen ( buf ) ;				// Get number of bytes to write
  stat = WriteFile ( hcom,				// Handle to the Serial port
                     buf,				// Data to be written to the port
                     nbToWrite,				// No of bytes to write
                     &nbWritten,			// Bytes written
                     NULL ) ;
  return ( stat && ( nbToWrite == nbWritten ) ) ;
}


//***************************************************************************************************
//					A V A I L A B L E					    * 
//***************************************************************************************************
// Check if there is input from the console.							    *
// Returns nonzero if input is available.							    *
//***************************************************************************************************
int available ( int fh )
{
  DWORD dwNumEvents;

  GetNumberOfConsoleInputEvents ( GetStdHandle ( fh ), &dwNumEvents ) ;
  return dwNumEvents ;
}


//***************************************************************************************************
//					R E A D C O N S						    *
//***************************************************************************************************
// Read a buffer from the console.								    *
//***************************************************************************************************
DWORD readcons ( char* buf, DWORD maxlen )
{
  int len = 0 ;						// Length of string

  if ( available ( STD_INPUT_HANDLE ) )			// Is there console input?
  {
    char* p = fgets ( buf, maxlen,			// Yes, read input
                      stdin ) ;
    if ( p == NULL )					// Read success?
    {
      fputs ( "read() of STDIN failed!\n",		// No, error!
              stderr ) ;
      return 0 ;					// Return bad result
    }
    len = strlen ( buf ) ;				// Get length of string
    p = p + len - 1 ;					// Point to line delimeter
    if ( *p == '\n' )					// Ends with linefeed?
    {
      *p = '\r' ;					// Yes, replace with CR
    }
  }
  return len ;						// Return length of input
}


//***************************************************************************************************
//					R E A D C O M						    *
//***************************************************************************************************
// Read a buffer from the serial port.								    *
//***************************************************************************************************
DWORD readcom ( char* buf, DWORD maxlen )
{
  BOOL  stat ;						// Result of read action
  DWORD nbRead ;					// Number of bytes read

  stat = ReadFile ( hcom,				// Handle to the Serial port
                    buf,				// Data to be written to the port
                    maxlen,				// Max number of bytes to read
                    &nbRead,				// Bytes actually read
                    NULL ) ;
  if ( stat )						// Success?
  {
    buf[nbRead] = '\0' ;				// Yes, delimit input line
    return nbRead ;					// Return number of bytes read
  }
  return -1 ;						// There was an error
}


void dumpo ( const char* id, const char* bf )
{
  printf ( "%s: ", id ) ;
  while ( *bf )
  {
    printf ( "%02X ", *bf++ ) ;
  }
  printf ( "\n" ) ;
}


//***************************************************************************************************
//					E C H O F I L T E R					    *
//***************************************************************************************************
// Filter out echoed characters.								    *
//***************************************************************************************************
char* echoFilter ( char* combuf, char* inbuf )
{
  while ( *combuf && *inbuf )
  {
    if ( *combuf != *inbuf )				// Differ fron sent string?
    {
      break ;						// Yes, return pointer to nonechoed char
    }
    combuf++ ;						// Echoed, bump pointers
    inbuf++ ;
  }
  return combuf ;					// Return shifted pointer
}


//***************************************************************************************************
//					P R I N T _ S E P					    *
//***************************************************************************************************
// Print a separating line.									    *
//***************************************************************************************************
void print_sep()
{
  const char* sep = "===============================" ;	// Separator line

  printf ( "%s%s%s\n", sep, sep, sep ) ;		// Draw separation line
}


//***************************************************************************************************
//			L I S T D I R E C T O R Y C O N T E N T S				    *
//***************************************************************************************************
// List all files on the directory specified by the paramater.					    *
//***************************************************************************************************
BOOL ListDirectoryContents ( const char* sDir )
{
    WIN32_FIND_DATA fdFile ;				// Result of Find file
    HANDLE          hFind = NULL ;			// File handle for Find
    char            sPath[256] ;

    sprintf ( sPath, "%s\\*.*", sDir ) ;    		// Specify a file mask. *.* = everything
    hFind = FindFirstFile ( sPath, &fdFile ) ;		// Find first file
    if ( hFind == INVALID_HANDLE_VALUE )		// Check result
    {
      printf ( "Path not found: [%s]\n", sDir ) ;	// Error!
      return FALSE ;
    }
    print_sep() ;					// Print separation line
    printf ( "Filename             Size\n" ) ;		// Header
    printf ( "-------------------- --------\n" ) ;
    do
    {
      if ( strcmp ( fdFile.cFileName, "."  ) != 0 &&	// Skip . and ..
           strcmp ( fdFile.cFileName, ".." ) != 0 )
      {
        // Build up our file path using the passed in
        //   [sDir] and the file/foldername we just found:
        // Is the entity a File or Folder?
        if ( fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
        {
          // sprintf ( sPath, "%s\\%s", sDir, fdFile.cFileName ) ;
          // printf ( "Directory: %s\n", sPath ) ;
          // ListDirectoryContents ( sPath ) ;		// Recursion!
          printf ( "%-20.20s    <dir>\n",		// Print directory name
                   fdFile.cFileName ) ;
        }
        else
        {
          printf ( "%-20.20s %8d\n",
                   fdFile.cFileName,			// Show name
                   fdFile.nFileSizeHigh << 16 |		// and size
                   fdFile.nFileSizeLow ) ;

        }
      }
    }
    while ( FindNextFile ( hFind, &fdFile ) ) ;		// Find the next file.
    FindClose ( hFind ) ;				// Clean things up!
    print_sep() ;					// Print separation line
    return TRUE ;
}



//***************************************************************************************************
//					B E A U T I F Y						    *
//***************************************************************************************************
// Shift the "ok\n" at the end of the line to the right margin.					    *
//***************************************************************************************************
void beautify ( char* buf, int margin )
{
  int len ;						// Length of string

  dumpo ( "Beautify", buf ) ;
  len = strlen ( buf ) ;				// Get length
  if ( ( tolower ( buf[len-3] ) == 'o' ) &&		// Ends with "ok" or "OK"?
       ( tolower ( buf[len-2] ) == 'k' ) &&
       ( buf[len-1] == '\n' ) )
  {
    while ( ( len = strlen ( buf ) ) < margin )		// Shift "ok" to the right
    {
      buf[len+1] = '\0' ;				// New delimeter
      buf[len-1] = buf[len-2] ;				// Shift the "k"
      buf[len-2] = buf[len-3] ;				// shift the "o"
      buf[len-3] = ' ' ;				// Insert a space
    }
  }
}


//***************************************************************************************************
//					I N C L U D E _ F I L E					    *
//***************************************************************************************************
// Include a source file and send it to the serial port.					    *
//***************************************************************************************************
BOOL include_file ( const char* filename )
{
  char  line[256] ;					// Input buffer for 1 line
  FILE* fp ;						// File handle
  int   len_1 ;						// Length of input string minus 1
  int   margin = 128 ;					// Margin for modified line

  fp = fopen ( filename, "r" ) ;			// Open the file
  if ( fp == NULL)					// Success?
  {
    printf ( "Unable to open %s!", filename ) ;		// No, show error
    return FALSE ;
  }
  com_timeout ( 700 ) ;					// Set long time-out
  print_sep() ;						// Print separation line
  while ( fgets ( line, sizeof(line), fp ) != NULL )
  {
    len_1 = strlen ( line ) - 1 ;			// Get length of line - 1
    if ( len_1 > 0 )					// Protect against empty lines
    {
      if ( line[len_1] == '\n' )			// Line ends with newline?
      {
        line[len_1] = '\r' ;				// Yes, change to CR
      }
      else
      {
        strcat ( line, "\r" ) ;				// No, append CR
      }
      //printf ( "line is %s\n", line ) ;
      if ( line[0] == '\\' )				// Starts with backslash?
      {
        continue ;					// Yes, skip
      }
      writecom ( line ) ;				// Send to com port
      Sleep ( 700 ) ;					// Give some time to handle input
      int n = readcom ( line, sizeof(line) ) ;		// Read reply from com port
      if ( n > 0 )					// Success?
      {
        if ( n > margin )				// Need to widen output ?
        {
          margin = n ;					// Yes
        }
        beautify ( line, margin ) ;
        printf ( "%s", line ) ;				// Print result
      }
    }
  }
  fclose ( fp ) ;					// Close input file
  print_sep() ;						// Print separation line
  com_timeout ( 50 ) ;					// Set default time-out
  return TRUE ;
}


//***************************************************************************************************
//					S H O W _ F I L E					    *
//***************************************************************************************************
// Show a source file.										    *
//***************************************************************************************************
BOOL show_file ( const char* filename )
{
  char        line[128] ;				// Input buffer for 1 line
  FILE*       fp ;					// File handle
  fp = fopen ( filename, "r" ) ;			// Open the file
  if ( fp == NULL)					// Success?
  {
    printf ( "Unable to open %s!", filename ) ;		// No, show error
    return FALSE ;
  }
  print_sep() ;						// Print separation line
  while ( fgets ( line, sizeof(line), fp ) != NULL )
  {
    printf ( "%s", line ) ;
  }
  fclose ( fp ) ;					// Close input file
  printf ( "\n" ) ;					// Extra newline
  print_sep() ;						// Print separation line
  return TRUE ;
}


//***************************************************************************************************
//					G E T T O K E N						    *
//***************************************************************************************************
// Get a token from the input string.  Parameter i is the index 0...n.				    *
//***************************************************************************************************
char* gettoken ( const char* str, int i )
{
  static char cstr[128] ;				// Copy of input string
  char        delim[] = " \r\n" ;			// List of delimeters
  char*       token ;					// Pointer to token

  strcpy ( cstr, str ) ;				// Make a copy of input string
  token = strtok ( cstr, delim ) ;			// Get first token, index=0
  while ( i-- > 0 && token )				// Search for requested token
  {
    token = strtok ( NULL, delim ) ;			// Get next token
  }
  return token ;					// Return requested token
}


//***************************************************************************************************
//				H A N D L E _ S P E C I A L_ 1					    *
//***************************************************************************************************
// Handle console commands starting with "#".							    *
// The next commands are supported:								    *
//   "ls"      -- List the files in the current directory.					    *
//   "cat"     -- Show a file.									    *
//   "include" -- Include file.  Send file to serial port.					    *
//   "i"       -- Same as "include".								    *
//***************************************************************************************************
void handle_special_1 ( const char* command )
{
  char* p ;						// Pointer to 2nd token
  char  dir[128] = "." ;				// Default directory

  p = gettoken ( command, 1 ) ;				// Get parameter (=filename)
  if ( strstr ( command, "ls" ) )			// "ls" command?
  {
    if ( p )						// Yes, directory given?
    {
      strcpy ( dir, p ) ;				// Yes, change default
    }
    strcat ( dir, "\\" ) ;				// Add backslash
    ListDirectoryContents ( dir ) ;			// Yes, list files on this directory
  }
  else if ( strstr ( command, "i" ) )			// "include" command?
  {
    if ( p )						// Yes, filename given?
    {
      include_file ( p ) ;				// Yes, include the file
    }
    else
    {
      printf ( "Filename missing!\n" ) ;		// No, show error
    }
  }
  else if ( strstr ( command, "cat" ) )			// "cat" command?
  {
    if ( p )						// Yes, filename given?
    {
      show_file ( p ) ;					// Yes, include the file
    }
    else
    {
      printf ( "Filename missing!\n" ) ;		// No, show error
    }
  }
}


//***************************************************************************************************
//				H A N D L E _ S P E C I A L_ 2					    *
//***************************************************************************************************
// Handle console commands starting with "\".							    *
// The next commands are supported:								    *
//   ???											    *
//***************************************************************************************************
void handle_special_2 ( const char* command )
{
  char* p ;						// Pointer to 2nd token

  p = gettoken ( command, 1 ) ;				// Get parameter (=filename)
  if ( strstr ( command, "test" ) )			// "test" command?
  {
  }
}


//***************************************************************************************************
//					M A I N							    *
//***************************************************************************************************
// Start of the main program.									    *
//***************************************************************************************************
int main ( int argc, char* argv[] )
{
  char combuf[100] ;					// Input from serial
  char inbuf[100] ;					// Input from console
  int  n ;

  parse_options ( argc, argv ) ;			// Parse commandline options
  printf ( 
      "e4thcom-" VERSION " on Windows.  "		// Show startup info
      "Serial Terminal for Embedded Forth Systems.\n"
      "Copyright (C) 2020 Ed Smallenburg.\n"
      "Conditions of the GNU General Public License "
      "with ABSOLUTELY NO WARRANTY.\n\n" ) ;
  if ( !open_port ( device ) )				// Open port for serial I/O to target
  {
    return -1 ;						// No success, leave main program
  }
  writecom ( "\r" ) ;					// Force Forth prompt
  while ( 1 )						// Main loop
  {
    n = 1 ;						// Force start of loop
    while ( n )
    {
      n = readcom ( combuf, sizeof(combuf) ) ;		// Try to read from serial
      if ( n < 0 )					// Input error?
      {
        fputs ( "read() from serial failed!\n",		// No, error
                stderr ) ;
      }
      else if ( n > 0 )					// Any input?
      {
        combuf[n] = '\0' ;				// Take care of delimeter
        char* p = echoFilter ( combuf, inbuf ) ;	// Remove echoed characters
        printf ( p ) ;					// Show to user
      }
    }
    if ( readcons ( inbuf, sizeof(inbuf) ) > 0 )	// Is there console input?
    {
      if ( inbuf[0] == '#' )				// Special input?
      {
        handle_special_1 ( inbuf + 1 ) ;		// Yes, handle it
        continue ;
      }
      if ( inbuf[0] == '\\' )				// Special input?
      {
        handle_special_2 ( inbuf + 1 ) ;		// Yes, handle it
        continue ;
      }
      writecom ( inbuf ) ; 
    }
  }
}