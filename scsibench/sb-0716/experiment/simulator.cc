#include <iostream.h>
#include <fstream.h>
#include <stdlib.h>
#include <stdio.h>

#define END_Z1 2580386 
#define END_ZN 17783239

#define DATA_SIZE 32*1024 // in kilobytes

#define ITERATIONS 1000

int main(int argc, char ** argv) {

  if(argc<2) {
    cout << "Usage : trace <BLOCKSIZE> <Z|F>" << endl ;
    exit(-1) ;
  }

  bool single_zone ;
  if(argv[1][0]=='Z') single_zone = true ;
  else single_zone = false ;
  
  const long BLOCK_SIZE = atol(argv[2]) ;

  long LBN_END ;
  if(single_zone) LBN_END = END_Z1 - BLOCK_SIZE ;
  else LBN_END = END_ZN - BLOCK_SIZE ;

  char outFile[30] ;
  sprintf(outFile, "z_%s_%ul", argv[1], BLOCK_SIZE);

  int iterations = ITERATIONS ;

  // Initialize output stream
  ofstream* out = new ofstream(outFile);
  if(!out->is_open()) {
    cerr << "Error opening trace file" << endl ;
    exit(-1);
  }          

  *out << "T 2" << endl ;
  
  long start ;
  while(iterations > 0) {

    start = lrand48() % LBN_END ;

    *out << "R " << start << " " << BLOCK_SIZE/512 << endl ;
    
    iterations-- ;

  }// while
  
  *out << "T 3" << endl ;
  *out << "C Total data read = " << ITERATIONS*BLOCK_SIZE/1024 << " KB" << endl ;
  *out << "- 0 3 2 microseconds" << endl ;

}// main
  
  
  
