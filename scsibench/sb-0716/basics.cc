#include "basics.h"
extern "C" {
#include <stdarg.h>
}

// bitprint to stderr for little endians
void bitprint(unsigned char* buf, int bytes) {

  for (int i=0; i < bytes; i++) {
    fprintf(stderr, "%d: ", i);
    for (int j=0; j < 8; j++) {
      fprintf(stderr, "%1.1x",
	      (unsigned char)(buf[i] >> (7-j)) & 0x1 );

      if ( j % 4 == 3 )
	fprintf(stderr, " ");
    }

    fprintf(stderr, "\t%2.2Xh\t%d\n", buf[i], (unsigned char)buf[i]);
  }

}

///////////////////////////////////
/// Exception
///////////////////////////////////

Exception::Exception(char *format, ...) {
  va_list args;

  va_start(args, format);

  msg = new char[1024];

  vsnprintf(msg, 1024, format, args);

  va_end(args);
}

Exception::Exception(const Exception& e) {
  msg = new char[ strlen(e.msg) ];
  strcpy(msg, e.msg);
}

Exception::~Exception() {
  if ( msg )
    delete[] msg;
  msg = NULL;
}

const char* Exception::getMsg() {
  if ( msg )
    return msg;
  else
    return "Exception";
}

