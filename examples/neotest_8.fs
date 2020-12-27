\ neotest.fs
\ Testing neopixels on D4
\ 14-12-2020, Ed Smallenburg
\ Protocol:
\ Per pixel 8 bytes are sent.  The order is Green, Red, Blue.  The most significant bit is the first to be sent.
\ "0" bits are sent as 375 nsec high followed by 938 nsec low.  "1" bits are sent as 750 nsec high
\ followed by 438 nsec low.
#require ]C!
#require ]B!
\res MCU: STM8S103
\res export PD_ODR PD_DDR PD_CR1 PD_CR2
nvm
  60 CONSTANT NEOS                          \ Number of neopixels
  NEOS 3 * CONSTANT NEOS*3                  \ Length of buffer
  VARIABLE neodata NEOS*3 allot             \ Buffer for n neopixels, 3 bytes per pixel
  VARIABLE stopreq                          \ Stop request if not 0


: pd4init ( -- )                            \ Initialize I/O for D4n is number of neopixels
  1 PD_DDR 4 B! 1 PD_CR1 4 B! 1 PD_CR2 4 B! \ Set D4 output push-pull
;

: neoshow ( -- )
  [ $9089 ,                                 \ PUSHW Y           // Save Y
    $89 C,                                  \ PUSHW X           // Save X
    $AE C, NEOS*3 ,                         \ LDW   X, #NEOS*3  // Index to end of data + 1
                              \ lb_byteloop:
    $5A C,                                  \ DECW  X           // Decrement index in data
    $2B2D ,                                 \ JRMI lb_exit      // Branch if all bytes handled
    $D6 C, NEODATA ,                        \ LD    A, (_Led,X) // Get next byte value
    $90AE , $0008 ,                         \ LDW Y, #08        // 8 bits per color
                              \ lb_bitloop:
    $7218 , PD_ODR ,                        \ BSET PD_ODR, #4   // Begin of pulse on D4
    $48 C,                                  \ SLL A             // Shift next bit to carry
    $2407 ,                                 \ JRNC lb_bit_0     // Jump for zero bit
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
                              \ lb_bit_0:
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $7219 , PD_ODR ,                        \ BRES PD_ODR, #4   // End of pulse on D4
    $2508 ,                                 \ JRC lb_bit_1      // Jump for one bit
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
                              \ lb_bit_1:
    $9D C,                                  \ NOP
    $9D C,                                  \ NOP
    $905A ,                                 \ DECW Y            // Count number of bits per byte
    $26DC ,                                 \ JRNE lb_bitloop   // Jump if more bits to do
    $20D0 ,                                 \ JRA ld_byteloop   // Next byte
                              \ lb_exit:
    $85 C,                                  \ POPW X
    $9085 ,                                 \ POPW Y
  ]
;


\ delay
\ delay for a number of milliseconds.
\ Timer "tim" runs on 200 Hz, so a tick is 5 millseconds.
: delay ( msec -- )
  5 / tim +                \ Compute end time
  stopreq @ 0=             \ Stop requested?
  if
    ?key if                \ No, key pressed?
      stopreq !            \ Yes use read character as stopflag
    then
    begin
      dup tim =            \ Loop until target is reached
    until
  then
  drop                     \ Drop delay time that was still on the stack
;


\ pxcolor
\ Set one pixel to a color
: pxcolor ( green red blue nr )
  NEOS 1 - min             \ Limit pixel number
  3 * neodata +            \ Compute pointer in neodata
  dup 4 pick swap C! 1+    \ Store green value of pixel
  dup 3 pick swap C! 1+    \ Store red value of pixel
  C!                       \ Store blue value of pixel
  drop drop                \ Cleanup stack
;


\ wheel ( pos -- red green blue )
\ Construct a color from wheel position (0..255).
: wheel
  255 and              \ Limit to 0..255
  255 swap -           \ Complement of position (cpos)
  dup 85 < if          \ cpos < 85 ?
    3 * dup            \ cpos * 3 needed 2 times
    255 swap -         \ red = 255 - cpos * 3
    0                  \ green = 0
    rot                \ blue = cpos * 3
  else
    dup 170 < if
      85 -             \ cpos = cpos - 85
      3 * dup          \ cpos * 3 needed 2 times
      0                \ red = 0
      rot              \ green = cpos * 3
      rot 255 swap -   \ blue = 255 - cpos * 3
    else
      170 -            \ cpos = cpos - 170
      3 * dup          \ cpos * 3 needed 2 times, red = cpos * 3
      255 swap -       \ green = 255 - cpos * 3
      0                \ blue = 0
    then
  then
;


\ colorwipe
\ Fill all pixels from end to begin. Speed is in the delay parameter.
: colorwipe ( green red blue delay -- )
  neodata NEOS 0           \ buffer and number of pixels to stack
  do
    dup 5 pick swap C! 1+  \ Get green value and store in neodata
    dup 4 pick swap C! 1+  \ Get red value and store in neodata
    dup 3 pick swap C! 1+  \ Get blue value and store in neodata
    1 pick delay           \ Delay for a while
    neoshow                \ Show neodata on LEDs
  loop
  drop drop drop drop drop \ Clean-up stack
;

\ thchase
\ Theater-style crawling lights
: thchase ( green red blue delay -- )
  10 0                     \ Do 10 cycle of chasing
  do
    3 0                    \ For 3 pixel shift
    do
      I                    \ Number of shifts (0..2) on the stack
      NEOS 0               \ For all pixels
      do                   \ Turn every 3rd pixel on
        4 pick             \ Get green value
        4 pick             \ Get red value
        4 pick             \ Get blue value
        3 pick I +         \ Get pixel number to access
        pxcolor            \ Set pixel to color
      3 +loop
      neoshow              \ Show the result
      1 pick delay         \ Wait some time
      NEOS 0               \ For all pixels
      do                   \ Turn every 3rd pixel off
        0 0 0              \ Green, red, blue off
        3 pick I +         \ Get pixel number to access
        pxcolor            \ Set pixel to color
      3 +loop
      drop                 \ Remove I
    loop
  loop
  drop drop drop drop
;


\ off ( -- )
\ Turn all pixels off
: off
  0 0 0 50 colorwipe \ All off
;


\ rainbow ( delay )
\ show rainbow.  Speed depends on delay parameter.
: rainbow
  256 0 do              \ All startcolors in the wheel
    I                   \ Loop variable to stack
    NEOS 0 do           \ loop for all pixels
      dup I + wheel     \ get wheel colours
      I pxcolor         \ Set colour
    loop
    drop                \ Remove I
    neoshow             \ Show result
    dup delay           \ Wait for some time
  loop
  drop                  \ Remove delay time from stack
;


\ crainbow ( delay )
\ show modified rainbow.  Speed depends on delay parameter.
: crainbow
  256 5 * 0 do                   \ 5 times all colors in the wheel
    I                            \ Loop variable to stack
    NEOS 0 do                    \ loop for all pixels
      dup I 256 * NEOS / + wheel \ get wheel colours
      I pxcolor                  \ Set colour
    loop
    drop                         \ Remove I
    neoshow                      \ Show result
    dup delay                    \ Wait for some time
  loop
  drop                           \ Remove delay time from stack
;


\ rthchase
\ Theater-style crawling lights whith rainbow effect
: rthchase ( delay -- )
  256 0                    \ Cycle all 256 colours in the wheel
  do
    I                      \ Number of colors to the stack
    3 0                    \ For 3 pixel shift
    do
      I                    \ Number of shifts (0..2) to the stack
      NEOS 0               \ For all pixels
      do                   \ Turn every 3rd pixel on
        dup 2 pick +       \ Color for wheel
        wheel              \ Get green, red, blue value
        3 pick I +         \ Get pixel number to access
        pxcolor            \ Set pixel to color
      3 +loop
      neoshow              \ Show the result
      2 pick delay         \ Wait some time
      NEOS 0               \ For all pixels
      do                   \ Turn every 3rd pixel off
        0 0 0              \ Green, red, blue off
        3 pick I +         \ Get pixel number to access
        pxcolor            \ Set pixel to color
      3 +loop
      drop                 \ Remove I
    loop
    drop                   \ Drop counter number of colors
  loop
  drop
;


: main
  pd4init cr                 \ Init I/O
  ." Stop loop with <Enter> key..."
  begin
      0 255   0 50 colorwipe \ Fill with red
    255   0   0 50 colorwipe \ Fill with green
      0   0 255 50 colorwipe \ Fill with blue
    off                      \ All off
    127 127 127 50 thchase   \ Theatre chase white
      0 127   0 50 thchase   \ Theatre chase red
      0   0 127 50 thchase   \ Theatre chase blue
    20 rainbow               \ Rainbow colors
    20 crainbow              \ Rainbow colors type 2
    50 rthchase              \ Theatre chase sliding colors
    off                      \ All off
    stopreq @ 0= not         \ Key pressed?
  until                      \ Loop if not
  drop                       \ Forget input character
  cr hi                      \ Show prompt
;

\ Take care to start main at boot time.
\
' main 'BOOT !

ram

