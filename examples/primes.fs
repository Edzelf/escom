\ primes.fs
\ Compute some primes
\ Will be stored in flash memory
NVM
\
\ sqrt ( square -- root )
\ Very rough square root approximation
\ Search for a number whose square is greater than the input number.
\ For speed reason, we start at 10
: sqrt 10 begin 1 + dup dup * 2 pick swap < until swap drop ;

\ isprime?. Find out if n is a prime number.  Input range is 3 to 32000.
\ if the number is prime, it is left on the stack, otherwise the result is 0.
: isprime? ( n -- b )
dup sqrt 3 do
dup I mod 0= if drop 0 leave then
2 +loop ;

\ Search primes for all odd odd numbers between 3 and 5000
\ For every prime found, the LED will flash.
: primes ( -- )
 2 . \ 2 is the first prime
 5000 3 do
   I isprime? dup 0= not if . 1 out! else 0 out! drop then
2 +loop cr 0 out! ;
RAM
