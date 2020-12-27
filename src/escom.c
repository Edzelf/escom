//***************************************************************************************************
//                              	E S C O M . C						    *
//***************************************************************************************************
// escom.c											    *
// Terminal program for embedded Forth Systems with interfaces that supports conditional and	    *
// unconditional uploading of source code.							    *
// Based on e4thcom by Manfred Mahlow.								    *
// The program was converted to C for Windows compatibility.					    *
// Can be compiled by the gcc compiler that is part of the Strawberry Perl for Windows package.     *
// See https://strawberryperl.com.								    *
// Compile command:                                                                                 *
//    gcc escom.c -o escom.exe									    *
// Save the resulting executive in a directory that is in your %PATH% for easy access.		    *
// Written by Ed Smallenburg.                                                                       *
// Todo:											    *
//   - Now, targets "stm8ef" and "mecrisp" are supported.  Add other platforms.			    *  
//***************************************************************************************************
// Command line options:									    *
//  -t xxxx	-- Target system, "stm8ef" and "mecrisp" are currently supported.	    	    *
//  -d xxxx	-- Communication device, for example "COM5".					    *
//  -b xxxx	-- Baudrate for communication, for example 115200.				    *
//  -p xxxx	-- Search path for #include, #require and \res files.				    *
// The option can also be defined in the escom.conf file in the user's home directory.		    *
//***************************************************************************************************
// escom reads lines from the terminal (with line editing).  Completed lines are forwarded to the   *
// target Forth device.										    *
// Lines starting with "#" or "\" are not forwarded, but interpreted by this program.		    *
// Configuration parameters are in escom.conf in the user HOME directory.			    *
// You may also specify command line parameters.  They will overrule the setting in the conf-file.  * 
//***************************************************************************************************
//                                                                                                  *
// Revision    Auth.  Remarks									    *
// ----------  -----  ----------------------------------------------------------------------------- *
// 18-12-2020  ES     Version 0.0,	First set-up.						    *
// 24-12-2020  ES     Version 0.1,  	Added recursive include.				    *
// 26-12-2020  ES     Version 0.1.1,	Added #cd command.					    *
// 27-12-2020  ES     Version 0.1.2,	Added mecrisp support.					    *
//***************************************************************************************************
#include <stdio.h>	// Console I/O
#include <stdlib.h>	// Standard library definitions
#include <string.h>	// String function definitions
#include <unistd.h>	// UNIX standard function definitions
#include <fcntl.h>	// File control definitions
#include <errno.h>	// Error number definitions
#include <windows.h>	// Windows specifics

// Constants:
#define VERSION "0.1.2"					// The version number
// Some textcolors
#define GREEN   ( FOREGROUND_GREEN | FOREGROUND_INTENSITY )
#define YELLOW  ( FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY )
#define RED     ( FOREGROUND_RED | FOREGROUND_INTENSITY )

struct dict_t						// Dictionary entry
{
  char symbol[20] ;					// Symbolic name
  WORD value ;						// Value
} ;

// Global variables
char          device[32] = "COM5" ;			// Default serial port for target connection
char          target[32] = "stm8ef" ;			// Default target system
int           baudrate = 9600 ;				// Default baudrate for communication
char          path[128] = ".;./mcu;./lib" ;		// Default search path for #i and #r	
HANDLE        hConsoleOut ;				// Handle for console output
HANDLE        hConsoleIn ;				// Handle for console input
HANDLE        hcom ;					// Handle for serial I/O
char*         ( *ok_chk ) ( char* buf ) ;		// Check reply string from target
int           tokc ;					// Number of tokens in tokv
char*         tokv[32] ;				// Tokens in config file
struct dict_t dictionary[1000] ;			// Escom dictionary
int           dinx = 0 ;				// Number of entries in dictionary

//***************************************************************************************************
//					C L E A R _ S C R E E N					    *
//***************************************************************************************************
// Clear console screen.									    *
//***************************************************************************************************
void clear_screen()
{
  CONSOLE_SCREEN_BUFFER_INFO csbi ;
  SMALL_RECT                 scrollRect ;
  COORD                      scrollTarget ;
  CHAR_INFO                  fill ;

  // Get the number of character cells in the current buffer.
  if ( !GetConsoleScreenBufferInfo ( hConsoleOut, &csbi ) )
  {
    return ;
  }
  // Scroll the rectangle of the entire buffer.
  scrollRect.Left = 0 ;
  scrollRect.Top = 0 ;
  scrollRect.Right = csbi.dwSize.X ;
  scrollRect.Bottom = csbi.dwSize.Y ;
  // Scroll it upwards off the top of the buffer with a magnitude of the entire height.
  scrollTarget.X = 0 ;
  scrollTarget.Y = (SHORT)( 0 - csbi.dwSize.Y ) ;
  // Fill with empty spaces with the buffer's default text attribute.
  fill.Char.UnicodeChar = TEXT ( ' ' ) ;
  fill.Attributes = csbi.wAttributes ;
  // Do the scroll
  ScrollConsoleScreenBuffer ( hConsoleOut, &scrollRect, NULL, scrollTarget, &fill ) ;
  // Move the cursor to the top left corner too.
  csbi.dwCursorPosition.X = 0 ;
  csbi.dwCursorPosition.Y = 0 ;
  SetConsoleCursorPosition ( hConsoleOut, csbi.dwCursorPosition ) ;
}


//***************************************************************************************************
//					T E X T _ A T T R					    *
//***************************************************************************************************
// Set text attributes, for example color.							    *
//***************************************************************************************************
void text_attr ( WORD attr )
{
  if ( attr == 0 )					// 0 is normal text
  {
    attr = FOREGROUND_BLUE | FOREGROUND_RED |		// For white text
              FOREGROUND_GREEN ;
  }
  SetConsoleTextAttribute ( hConsoleOut, attr ) ;
}


//***************************************************************************************************
//				U S E R _ E R R O R						    *
//***************************************************************************************************
// Show error on console in red.								    *
//***************************************************************************************************
void user_error ( const char* format, ... )
{
  static char sbuf[64] ;				// For text with error
  va_list     varArgs ; 				// For variable number of params

  va_start ( varArgs, format ) ;			// Prepare parameters
  vsnprintf ( sbuf, sizeof(sbuf), format, varArgs ) ;	// Format the message
  va_end ( varArgs ) ;					// End of using parameters
  text_attr ( RED ) ;					// Print error in red
  printf ( "%s!\n", sbuf ) ;				// Show error
  text_attr ( 0 ) ;					// Back to normal colors
}


//***************************************************************************************************
//			T O K E N I Z E _ C O N F _ F I L E					    *
//***************************************************************************************************
// Parse the options in the config file.							    *
// Results will be in tokc and tokv.								    *
//***************************************************************************************************
void tokenize_conf_file()
{
  char        exename[32] ;				// Name of executable/config file
  char        filepath[128] ;				// Will be path of config file
  char        line[128] ;				// Input buffer for 1 line
  FILE*       fp ;					// File handle
  char        value[128] ;				// Value of option
  char*       p ;					// Point to token in pool

  GetModuleFileName ( NULL, filepath,			// Get path of executable
                      sizeof(filepath) ) ;
  strcpy ( exename, strrchr( filepath, '\\' ) ) ;	// Isolate executable name
  strcpy ( strrchr ( exename, '.' ), ".conf" ) ;	// Replace .exe with .conf
  strcpy ( filepath, getenv ( "USERPROFILE" ) ) ;	// Get user home directory
  strcat ( filepath, exename ) ;			// Add name of config file
  fp = fopen ( filepath, "r" ) ;			// Open the file
  if ( fp == NULL)					// Success?
  {
    user_error ( "Unable to open %s", filepath ) ;	// No, show error
    return ;
  }
  tokc = 0 ;						// Number of tokens
  tokv[tokc++] = ".conf" ;				// First token is dummy
  while ( fgets ( line, sizeof(line), fp ) != NULL )
  {
    if ( line[0] == '-' )				// Line contains an option?
    {
      line[2] = '\0' ;					// Delimter after option
      p = (char*)malloc ( 4 ) ;				// Create space for option token
      strcpy ( p, line ) ;				// Save option in pool 
      tokv[tokc++] = p ;				// Add to tokv
      p = strtok ( line + 3, " \t\n\r" ) ;		// Get value of parameter
      if ( p )						// Does it have a value
      {
        strcpy ( value, p ) ;				// Get value of parameter
        p = (char*)malloc ( strlen(value) ) ;		// Create space for value token
        strcpy ( p, value ) ;
        tokv[tokc++] = p ;				// Add to tokv
      }
    }
  }
  fclose ( fp ) ;					// Close input file
  printf ( "\n" ) ;					// Extra newline
}


//***************************************************************************************************
//				P A R S E _ O P T I O N						    *
//***************************************************************************************************
// Parse the commandline options.								    *
// Called for options in config file and foroptions on commandline.				    * 
//***************************************************************************************************
void parse_options ( int argc, char* argv[] )
{
  const char* opts = "d:b:t:p:" ;				// Options allowed
  int         optchar ;						// Option found
  int         baudrates[] = { CBR_9600,   CBR_14400,		// Allowed baudrates
                              CBR_19200,  CBR_38400,
                              CBR_56000,  CBR_57600,
                              CBR_115200, CBR_128000 } ;
  int b ;							// Baudrate in option

  optind = 1 ;							// Start with first option
  while ( ( optchar = getopt ( argc, argv, opts ) ) != -1 )	// Get next option
  {
    switch ( optchar )
    {
      case 'd' :						// Device spec?
        strncpy ( device, optarg, sizeof(device) - 1 ) ;	// Yes set device
        break;
      case 'b' :						// Baudrate?
        b = atoi ( optarg ) ;					// Yes, get value
        int n = sizeof(baudrates) / sizeof(int) ;		// Number of possible baudrates
        BOOL found = FALSE ;					// Assume not found
        for ( int i = 0 ; i < n ; i++ )				// Check for legal baudrate
        {
          if ( b == baudrates[i] )				// Equal to this entry?
          {
            found = TRUE ;					// Yes, mark as found
            break ;						// No need to continue
          }
        }
        if ( found )						// Legal baudrate found?
        { 
          baudrate = b ;					// Yes, set baudrate
        }
        else
        {
          user_error ( "Illegal baudrate %d in option", b ) ;	// No, warning
        }
        break;
      case 't':							// Target system?
        strncpy ( target, optarg, sizeof(target) - 1 ) ;	// Yes set device
        break;
      case 'p' :						// Search pathg?
        strncpy ( path, optarg, sizeof(path) - 1 ) ;		// Yes, set search path
        break ;
    }
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
  timeouts.ReadIntervalTimeout         = 0 ;		// in milliseconds
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
    user_error ( "Error in opening %s", port ) ;	// No success
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
int available()
{
  DWORD dwNumEvents;

  GetNumberOfConsoleInputEvents ( hConsoleIn, &dwNumEvents ) ;
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

  if ( available() )					// Is there console input?
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
int readcom ( char* buf, DWORD maxlen, int maxtry )
{
  BOOL  stat ;						// Result of read action
  DWORD nbRead ;					// Number of bytes read
  int   totread = 0 ;					// Total bytes read
  char* p = buf ;					// Fill pointer
  BOOL  eol ;						// End of line seen

  while ( TRUE )
  {
    stat = ReadFile ( hcom,				// Handle to the Serial port
                      p,				// Data to be read from the port
                      maxlen,				// Max number of bytes to read
                      &nbRead,				// Bytes actually read
                      NULL ) ;
    if ( ! stat )
    {
      return -1 ;
    }
    p += nbRead ;
    totread += nbRead ;
    maxlen -= nbRead ;
    *p = '\0' ;						// Force end of buffer
    eol = ( buf[totread - 1] == '\n' ) |		// End of input?
          ( buf[totread - 1] == '\r' ) ;
    if ( eol )
    {
      break ;						// End of input: exit loop
    }
    if ( --maxtry == 0 )
    {
      break ;
    }
  }
  return totread ;					// Return number of bytes read
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
  if ( hFind == INVALID_HANDLE_VALUE )			// Check result
  {
    user_error ( "Path not found: [%s]", sDir ) ;	// Error!
    return FALSE ;
  }
  print_sep() ;						// Print separation line
  printf ( "Filename                 Size\n" ) ;	// Header
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
        printf ( "%-20.20s    <dir>\n",			// Print directory name
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
  FindClose ( hFind ) ;					// Clean things up!
  print_sep() ;						// Print separation line
  return TRUE ;
}


//***************************************************************************************************
//				C H E C K _ O K _ S T M 8 E					    *
//***************************************************************************************************
// Check for "ok\n" at the end of the line.							    *
// Version for stm8e.										    *
//***************************************************************************************************
char* check_ok_stm8e ( char* buf )
{
  char* res ;						// Function result

  res = buf += strlen ( buf ) - 3 ;			// Points to end of string - 3
  if ( ( tolower ( *buf++ ) != 'o' ) ||		        // Ends with "ok" or "OK"?
       ( tolower ( *buf++ ) != 'k' ) ||
       ( *buf != '\n' ) )
  {
    res = NULL ;					// Not expected end
  }
  return res ;
}


//***************************************************************************************************
//				C H E C K _ O K _ M E C R I S P					    *
//***************************************************************************************************
// Check for "ok\n" at the end of the line.							    *
// Version for mecrisp.										    *
//***************************************************************************************************
char* check_ok_mecrisp ( char* buf )
{
  char* res ;						// Function result

  res = ( buf += strlen ( buf ) - 4 ) ;			// Points to end of string - 4
  if ( ( tolower ( *buf++ ) != 'o' ) ||		        // Ends with "ok" or "OK"?
       ( tolower ( *buf++ ) != 'k' ) ||
       ( *buf++ != '.' ) ||
       ( *buf != '\n' ) )
  {
    res = NULL ;					// Not expected end
  }
  return res ;
}


//***************************************************************************************************
//					B E A U T I F Y						    *
//***************************************************************************************************
// Shift the "ok[.]\n" at the end of the line to the right margin.				    *
//***************************************************************************************************
void beautify ( char* buf, int margin )
{
  char   okbuf[8] ;					// Last part of string
  int    lenok  ;					// Length of last part
  int    len ;						// Length of string
  char*  p ;						// Will point to "ok" in string
  int    n ;						// Number of bytes to move

  if ( ( p = ok_chk ( buf ) ) )				// Ends with "ok" phrase?
  {
    strcpy ( okbuf, p ) ;				// Yes, save last part
    lenok = strlen ( okbuf ) ;				// Get length of last part
    *p = '\0' ;						// Shorten first part			
    while ( ( strlen ( buf ) + lenok ) < margin )	// Shift "ok" phrase to the right
    {
      strcat ( buf, " " ) ;				// Extend first part by one space
    }
    strcat ( buf, okbuf ) ;				// Add last part
  }
}


BOOL fileExists ( const char* fspec )
{
  DWORD dwAttrib ;

  dwAttrib = GetFileAttributes ( fspec ) ;
  return ( dwAttrib != INVALID_FILE_ATTRIBUTES && 
         !( dwAttrib & FILE_ATTRIBUTE_DIRECTORY ) ) ;
}


//***************************************************************************************************
//					S E A R C H _ F I L E					    *
//***************************************************************************************************
// Search for a file in de configured path.  If the filename contains a "/", the path is not used.  *
// If the file is not found, a NULL is returned.						    *
//***************************************************************************************************
const char* search_file ( const char* fnam )
{
  char        mypath[sizeof(path)] ;			// Copy of search path
  char*       token ;					// Pointer to token (= entry in path)
  static char sfile[128] ;				// Full spec of file to search

  if ( fileExists ( fnam ) )				// Try the simple one
  {
    return fnam ;
  }
  if ( strchr ( fnam, '/' ) )				// Slash in filename?
  {
    return NULL ;					// Yes, do not search
  }
  if ( strchr ( fnam, '\\' ) )				// Backslash in filename?
  {
    return NULL ;					// Yes, do not search
  }
  // Now try all entries in the search path
  strcpy ( mypath, path ) ;				// Make copy of path
  token = strtok ( mypath, ";" ) ;			// Get first token (=directory)
  while ( token )					// Search for requested token
  {
    strcpy ( sfile, token ) ;				// First part of ful file spec
    strcat ( sfile, "/" ) ;				// Add slash
    strcat ( sfile, fnam ) ;				// Add filename
    if ( fileExists ( sfile ) )				// See if it exists
    {
      return sfile ;					// Yes, return full spec
    }
    token = strtok ( NULL, ";" ) ;			// No, get next token
  }
  return NULL ;						// File not found
}



//***************************************************************************************************
//					G E T T O K E N						    *
//***************************************************************************************************
// Get a token from the input string.  Parameter i is the index 0...n.				    *
//***************************************************************************************************
const char* gettoken ( const char* str, int i )
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
//					S T R I P _ C O M M E N T				    *
//***************************************************************************************************
// Strip comment at end of line.								    *
//***************************************************************************************************
void strip_comment ( char *line )
{
  char* p ;						// Result of strrchr()

  while ( ( p = strrchr ( line, '\\' ) ) > line )	// Comment at the end of the line?
  {
    *p++ = '\r' ;					// Yes, force end of line
    *p = '\0' ;
  }
}


//***************************************************************************************************
//					S E A R C H _ D I C T					    *
//***************************************************************************************************
// Search for a symbol in the dictionary.							    *
// Index will be returned, or -1 if not found.							    *
//***************************************************************************************************
int search_dict ( const char* symbol )
{
  int i ;						// Index in dictionary

  for ( i = 0 ; i < dinx ; i++ )			// Search in dictionary
  {
    if ( strcmp ( dictionary[i].symbol, symbol ) == 0 )	// Match?
    {
      return i ;					// Yes, return index
    }
  }
  return -1 ;						// Symbol not found
}


//***************************************************************************************************
//				L O A D _ C P U _ R E S O U R C E S				    *
//***************************************************************************************************
// Load the CPU symbols in the dictionary.							    *
//***************************************************************************************************
BOOL load_cpu_res ( const char* filespec )
{
  FILE*       fp = NULL ;				// File handle
  char        line[128] ;				// Input buffer for 1 line
  int         v ;					// Value of symbol
  const char* symbol ;					// Points to symbol name
  int         i ;					// Index of found symbol

  fp = fopen ( filespec, "r" ) ;			// Open the file
  if ( fp == NULL)					// Success?
  {
    user_error ( "Unable to open %s", filespec ) ;	// No, show error
    return FALSE ;
  }
  while ( fgets ( line, sizeof(line), fp ) != NULL )	// Read next line from file
  {
    if ( line[0] == '\\' )				// Skip comment lines
    {
      continue ;
    }
    if ( gettoken ( line, 2 ) == NULL )			// At least 3 token in the line?
    {
      continue ;					// No, skip line
    }
    if ( strcmp ( gettoken ( line, 1 ), "equ" ) == 0 )	// Is this an "equ" line?
    {
      v = strtol ( gettoken ( line, 0 ), NULL, 16 ) ;	// Convert hexadecimal value
      symbol = gettoken ( line, 2 ) ;			// Get pointer to symbol name
      i = search_dict ( symbol ) ;			// Search in dictionary
      if ( i < 0 )					// Already in dictionary?
      {
        strcpy ( dictionary[dinx].symbol, symbol ) ;	// No, symbol name to dictionary entry
        dictionary[dinx].value = v ;			// Store in new dictionary entry
        dinx++ ;					// Update index (next to fill)
      }
      else
      {
        dictionary[i].value = v ;			// Overwrite value in dictionary entry
      }
    }
  }
  fclose ( fp ) ;					// Close input file
  return TRUE ;						// Positive result
}


//***************************************************************************************************
//					F O R T H _ C H E C K					    *
//***************************************************************************************************
// Send line to target and check the result.							    *
//***************************************************************************************************
BOOL forth_check ( const char* teststr )
{
  char line[128] ;

  writecom ( teststr ) ;				// Send to target
  readcom ( line, sizeof(line), 12 ) ;			// Read reply from com port
  return ( strchr ( line, 0x07 ) == NULL ) ;		// BELL in the string means error
}


//***************************************************************************************************
//					H A N D L E _ R E S					    *
//***************************************************************************************************
// Handle the "\res" lines.									    *
// Examples:											    *
//   \res MCU: STM8S103				-- Loads the (resource) file STM8S103.efr into the  *
//						   escom dictionary.  The resource file must be in  *
//						   the path specified by the -p option.		    *
//						   line in the .efr file look like this:	    *
//							7F60 equ CFG_GCR    \ Global config...	    *
//   \res <single-number> equ name		-- Defines a new symbolic name (with value) in the  *
//						   escom dictionary.  The number is treated as	    *
//						   hexadecimal.					    *
//   \res export PD_ODR PD_DDR PD_CR1 PD_CR2	-- Exports symbolic names in the dictionary to the  *
//						   target as constants.				    *
//***************************************************************************************************
BOOL handle_res ( const char* line )
{
  char        cpu[32] ;					// CPU name token in copy of line
  char        expstr[64] ;				// String to target for export constant
  const char* p ;					// Points full path of cpu .efr file
  FILE*       fp = NULL ;				// File handle
  int         tinx ;					// Token index in line
  const char* symbol ;					// Point to next symbol in export line
  int         v ;					// Value of symbol
  int         inx ;					// Index in dictionary found symbol
  BOOL        result = TRUE ;				// Function result

  text_attr ( GREEN ) ;					// Info in green
  printf ( "\\res" ) ;					// Show first part of command in green
  text_attr ( 0 ) ;					// Rest in normal color
  printf ( "%s\n", line + 4 ) ;				// Show rest of line
  if ( strcasecmp ( gettoken ( line, 1 ),		// MCU spec?
                    "MCU:" ) == 0 )
  {
    strcpy ( cpu, gettoken ( line, 2 ) ) ;		// Yes, get CPU name like "STM8S103"
    strcat ( cpu, ".efr" ) ;				// Fixed extension
    p = search_file ( cpu ) ;				// Search file in path
    if ( p == NULL )					// Check if file exists
    {
      user_error ( "%s not found", cpu ) ;		// Not existing, show error
      result = FALSE ;					// Return bad result
    }
    else
    {
      load_cpu_res ( p ) ;				// Store symbols in dictionary 
    }
  }
  else if ( strcasecmp ( gettoken ( line, 1 ),		// Export symbol(s)?
                         "export" ) == 0 )
  {
    tinx = 2 ;						// Start at first symbol
    while ( ( symbol = gettoken ( line, tinx++ ) ) )	// Get next symbol
    {
      inx = search_dict ( symbol ) ;			// Search in dictionary
      if ( inx < 0 )					// Symbol found?
      {
        result = FALSE ;				// No, error
        break ;
      }
      sprintf ( expstr, "' %s DROP\r", symbol ) ;	// Try to get symbol as a word
      if ( ! forth_check ( expstr ) )			// If this fails..
      {
        v = dictionary[inx].value ;			// Get value
        sprintf ( expstr, "$%X CONSTANT %s\r",		// Add the constant
                  v, symbol ) ;
	if ( ! forth_check ( expstr ) )			// Success?
	{
          result = FALSE ;
          break ;
        }
      }
    }
  }
  else if ( strcasecmp ( gettoken ( line, 2 ),		// Single symbol?
                         "equ" ) == 0 )
  {
    symbol = gettoken ( line, 3 ) ;			// Yes, get symbol
    v = strtol ( gettoken ( line, 1 ), NULL, 16 ) ;	// Convert hexadecimal value
    if ( symbol )					// Make sure symbol found
    {
      tinx = search_dict ( symbol ) ;			// Symbol already defined?
      if ( tinx < 0 )
      {
        strcpy ( dictionary[dinx].symbol, symbol ) ;	// No, symbol name to dictionary entry
        dictionary[dinx].value = v ;			// Store in new dictionary entry
        dinx++ ;					// Update index (next to fill)
      }
      else
      {
        dictionary[tinx].value = v ;			// Overwrite value in dictionary entry
      }
    }      
  }
  return result ;
}


//***************************************************************************************************
//					I N C L U D E _ F I L E					    *
//***************************************************************************************************
// Include a source file and send it to the serial port.					    *
// Works also for conditonal include ( #require ).						    *
// May be called recursively.									    *
//***************************************************************************************************
BOOL include_file ( const char* filename, BOOL conditional )
{
  char        line[256] ;				// Input buffer for 1 line
  FILE*       fp = NULL ;				// File handle
  int         len_1 ;					// Length of input string minus 1
  int         margin = 85 ;				// Margin for modified line
  char        myfile[sizeof(path)] ;			// Full filespec
  const char* p ;					// For recursive include file
  const char* word ;					// Points to filename as word
  char        wordtest[32] ;				// Test for existing word
  BOOL        rcond ;					// Recursive conditional
  BOOL        result = TRUE ;				// Function result
 
  p = search_file ( filename ) ;			// Search file in path
  if ( p  )						// Found?
  {
    strcpy ( myfile, p ) ;				// Filename now with full path
    fp = fopen ( myfile, "r" ) ;			// Open the file
  }
  if ( fp == NULL)					// Success?
  {
    user_error ( "Unable to open %s", filename ) ;	// No, show error
    return FALSE ;
  }
  if ( conditional )					// Was it an "require"
  {
    word = strrchr ( filename, '\\' ) ;		  	// Yes, isolate filename
    if ( word == NULL )					// Backslash in filename?
    {
      word = strrchr ( filename, '/' ) ;		// No, try forward slash
    }
    if ( word == NULL )					// Was there a (back)slash?
    {
      word = filename ;					// No, use plain filename
    }
    else
    {
      word++ ;						// Skip over (back)slash
    }
    sprintf ( wordtest, "\' %s DROP\r", word ) ;	// Format a test for this word
    if ( forth_check ( wordtest ) )			// Send to target and check result
    {
      fclose ( fp ) ;					// Word existing, close input file
      return TRUE ;					// and exit
    }
  }
  // File must be included.
  print_sep() ;						// Print separation line
  text_attr ( YELLOW ) ;				// Info in yellow
  printf ( "Uploading %s\n\n", myfile ) ;		// Show info
  text_attr ( 0 ) ;					// Normal text
  while ( fgets ( line, sizeof(line), fp ) != NULL )	// Read next line from file
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
      if ( strstr ( line, "\\\\" ) == line )		// Line starts with double backslash?
      {
        break ;						// Yes, skip rest of file
      }
      if ( strstr ( line, "\\res" ) == line )		// Line starts with "\res"?
      {
        result = handle_res ( line ) ;			// Yes, handle it
        if ( ! result )					// Check result
        {
          break ;					// Error, stop
        }
        continue ;					// Next line
      }
      if ( line[0] == '\\' )				// Starts with backslash?
      {
        text_attr ( YELLOW ) ;				// Show comments in yellow
        printf ( "%s\n", line ) ;			// Show comment line
        text_attr ( 0 ) ;				// Back to normal
        continue ;					// Yes, skip
      }
      rcond = ( strstr ( line, "#require" ) == line ) ;	// Starts with "#require"?
      if ( rcond ||
           strstr ( line, "#include" ) == line )	// Or starts with "#include"?
      {
        text_attr ( GREEN ) ;				// Show line in green
        writecom ( line ) ;				// Send to com port
        text_attr ( 0 ) ;				// Color back to normal
        p = gettoken ( line, 1 ) ;			// Get parameter (=filename)
        if ( p )					// Filename supplied?
        {
          result = include_file ( p, rcond ) ;		// Include recursively
          if ( ! result )				// Check result
          {
            break ;					// Error, stop
          }
        }
        continue ;					// No error, go to next line
      }
      strip_comment ( line ) ;				// Strip off comments at end of line
      writecom ( line ) ;				// Send to com port
      int n = readcom ( line, sizeof(line), 12 ) ;	// Read reply from com port
      if ( n > 0 )					// Success?
      {
        if ( n > margin )				// Need to widen output ?
        {
          margin = n ;					// Yes
        }
        beautify ( line, margin ) ;
        printf ( "%s", line ) ;				// Print result
        if ( strchr ( line, 0x07 ) )			// BELL in the string means error
        {
          user_error ( "\nError, abort upload" ) ;	// Show error
          break ;
        }
      }
    }
    else						// Empty line
    {
      printf ( "\n" ) ;					// Show empty line
    }
  }
  text_attr ( YELLOW ) ;				// Info in yellow
  printf ( "\nClosing %s\n", myfile ) ;			// Show info
  text_attr ( 0 ) ;					// Normal text
  fclose ( fp ) ;					// Close input file
  print_sep() ;						// Print separation line
  return result ;					// Return the result
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
    user_error ( "Unable to open %s", filename ) ;	// No, show error
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
//				H A N D L E _ S P E C I A L					    *
//***************************************************************************************************
// Handle console commands starting with "#".							    *
// The next commands are supported:								    *
//   "ls"      -- List the files in the current directory.					    *
//   "dir"     -- Same as "ls".									    *
//   "cat"     -- Show a file.									    *
//   "cd"      -- Change working directory.							    *
//   "include" -- Include file.  Send file to serial port.					    *
//   "i"       -- Same as "include".								    *
//   "require" -- Insert file if word does not yet exist on the target device.			    *
//   "r"       -- Same as "require".								    *
//***************************************************************************************************
void handle_special ( const char* command )
{
  const char* p ;					// Pointer to 2nd token
  char        dir[128] = "." ;				// Default directory
  const char* fm = "Filename missing" ;			// Common error

  p = gettoken ( command, 1 ) ;				// Get parameter (path/filename)
  if ( ( strstr ( command, "ls" ) == command ) ||	// "ls" command?
       ( strstr ( command, "dir" ) == command ) )	// or "dir" command?
  {
    if ( p )						// Yes, directory given?
    {
      strcpy ( dir, p ) ;				// Yes, change default
    }
    strcat ( dir, "\\" ) ;				// Add backslash
    ListDirectoryContents ( dir ) ;			// Yes, list files on this directory
  }
  else if ( strstr ( command, "cd" ) == command )	// "cd" command?
  {
    if ( ( p == NULL ) ||				// Yes, directory given?
         ( ! SetCurrentDirectory ( p ) ) )		// Yes, try to change directory
    {
      user_error ( "Directory does not exist" ) ;	// Did not work
    }
  }
  else if ( strstr ( command, "i" )  == command )	// "include" command?
  {
    if ( p )						// Yes, filename given?
    {
      include_file ( p, FALSE ) ;			// Yes, include the file
    }
    else
    {
      user_error ( fm ) ;				// No, show error
    }
  }
  else if ( strstr ( command, "r" ) == command )	// "r" or "require" command?
  {
    if ( p )						// Yes, filename given?
    {
      include_file ( p, TRUE ) ;			// Yes, include the file conditional
    }
    else
    {
      user_error ( fm ) ;				// No, show error
    }
  }
  else if ( strstr ( command, "cat" ) == command )	// "cat" command?
  {
    if ( p )						// Yes, filename given?
    {
      show_file ( p ) ;					// Yes, include the file
    }
    else
    {
      user_error ( fm ) ;				// No, show error
    }
  }
  writecom ( "\r" ) ;					// Force Forth prompt
}


//***************************************************************************************************
//				S E T _ T A R G E T _ S P E C I A L S				    *
//***************************************************************************************************
// Set target dependant stuff.									    *
//***************************************************************************************************
void set_target_specials()
{
  // Set the check function for the "OK" replay of the target.
  ok_chk = &check_ok_stm8e ;				// Assume target is "stm8ef"
  if ( strcasecmp ( target, "mecrisp" ) == 0 )		// Target is "mecrisp" ?
  {
    ok_chk = &check_ok_mecrisp ;			// Yes, use mecrist version
  }
}


//***************************************************************************************************
//					M A I N							    *
//***************************************************************************************************
// Start of the main program.									    *
//***************************************************************************************************
int main ( int argc, char* argv[] )
{
  char   combuf[256] ;					// Input from serial
  char   inbuf[128] ;					// Input from console
  int    n ;

  hConsoleOut = GetStdHandle ( STD_OUTPUT_HANDLE ) ;	// Get handles for console
  hConsoleIn =  GetStdHandle ( STD_INPUT_HANDLE ) ;	// output and input
  clear_screen() ;
  tokenize_conf_file() ;				// Read option in config file
  parse_options ( tokc, tokv ) ;			// Parse config options
  parse_options ( argc, argv ) ;			// Parse commandline options
  set_target_specials() ;				// Set target dependant things
  text_attr ( YELLOW ) ;				// Yellow text
  printf (
      "escom-" VERSION " : "				// Show startup info
      "Serial Terminal for "
      "Embedded Forth Systems.\n" ) ;
  text_attr ( 0 ) ;					// Normal text
  printf (
      "Copyright (C) 2020 Ed Smallenburg. "
      "This is free software under the\n"
      "conditions of the GNU General Public License "
      "with ABSOLUTELY NO WARRANTY.\n\n" ) ;
  printf ( "Active options:\n" ) ;
  printf ( "-d (PORT    ) - %s\n", device ) ;		// Serial port configured
  printf ( "-b (BAUDRATE) - %d\n", baudrate ) ;		// Baudrate configured
  printf ( "-t (TARGET  ) - %s\n", target ) ;		// Target system configured
  printf ( "-p (PATH    ) - %s\n", path ) ;		// Search path configured
  print_sep() ;						// Print separator
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
      n = readcom ( combuf, sizeof(combuf) - 1, 1 ) ;	// Try to read from serial
      if ( n < 0 )					// Input error?
      {
        fputs ( "read() from serial failed!\n",		// No, error
                stderr ) ;
      }
      else if ( n > 0 )					// Any input?
      {
        combuf[n] = '\0' ;				// Force delimiter
        char* p = echoFilter ( combuf, inbuf ) ;	// Remove echoed characters
        printf ( p ) ;					// Show to user
      }
    }
    if ( readcons ( inbuf, sizeof(inbuf) ) > 0 )	// Is there console input?
    {
      if ( inbuf[0] == '#' )				// Special input?
      {
        handle_special ( inbuf + 1 ) ;			// Yes, handle it
        continue ;
      }
      else if ( inbuf[0] == '\\' )			// Special input?
      {
        break ;						// End program
      }
      writecom ( inbuf ) ; 				// Forward to serial output
    }
  }
}
