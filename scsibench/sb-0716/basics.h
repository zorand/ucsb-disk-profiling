/*
 * Revision 0.2 - 2/20/2001
 * Scsibench - David & Zoran
 */

#ifndef BASICS_H
#define BASICS_H

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
}

inline void skip2nl(FILE *f){ 
  while(fgetc(f)!='\n'){
    if(feof(f)) break;
  }
} 

// Time stamp register library
extern "C" {
#include "time_cnt/tsc_time.h"
}

// For debugging
#define ASSERT(tst) if (!(tst)) { throw new Exception("ASSERTION FAILED at %s:%d", __FILE__, __LINE__); }


// Byte convertion macros
#define UCHAR4_TO_UINT(buffer) (((unsigned) (buffer)[0]) << 24 | \
                                ((unsigned) (buffer)[1]) << 16 | \
                                ((unsigned) (buffer)[2]) << 8  | \
                                ((unsigned) (buffer)[3]))

#define UCHAR3_TO_UINT(buffer) (((unsigned) (buffer)[0]) << 16 | \
                                ((unsigned) (buffer)[1]) << 8  | \
                                ((unsigned) (buffer)[2]))

#define UCHAR2_TO_UINT(buffer) (((unsigned) (buffer)[0]) << 8 | \
                                ((unsigned) (buffer)[1]))

#define SET_UCHAR4_FROM_UINT(buffer,value) { \
       (buffer)[0] =  (unsigned char) ((value) >> 24); /* MSB */ \
       (buffer)[1] =  (unsigned char) ((value) >> 16);           \
       (buffer)[2] =  (unsigned char) ((value) >> 8);            \
       (buffer)[3] =  (unsigned char) (value);         /* LSB */ }

#define SET_UCHAR3_FROM_UINT(buffer,value) { \
       (buffer)[0] =  (unsigned char) ((value) >> 16); /* MSB */ \
       (buffer)[1] =  (unsigned char) ((value) >> 8);            \
       (buffer)[2] =  (unsigned char) (value);         /* LSB */ }

#define SET_UCHAR2_FROM_UINT(buffer,value) { \
       (buffer)[0] =  (unsigned char) ((value) >> 8); /* MSB */ \
       (buffer)[1] =  (unsigned char) (value);        /* LSB */ }

// set the ith bit of buffer to value
#define SET_UCHAR_FROM_BIT(buffer,bit,value) { \
       (buffer)[0] = ( (buffer)[0] & ~(1 << (bit)) ) | \
                     ( (value) ? (1 << (bit)) : 0 ); }

// define GETTIME to use time_cnt/tsc_time.h library
#define GETTIME() rdtsc()

#define TRUEFALSE(x) (x?"True":"False")

#define BLOCK_SIZE 512

#define MIN(a,b) ( (a < b) ? a : b )
#define MAX(a,b) ( (a > b) ? a : b )

#define TRUE 1
#define FALSE 0

extern void bitprint(unsigned char* buf, int bytes);

// This is the Exception class
//
// When raising an exception, always throw an Exception pointer.
// The final catcher should delete the exception.
class Exception {
 public:
  Exception(char *format, ...);
  Exception(const Exception&);
  ~Exception();

  const char* getMsg();

 protected:  
  char *msg;
};

#endif //BASICS_H
