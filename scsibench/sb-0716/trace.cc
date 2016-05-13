#include "scsibench.h"

// --------------------------------------------
// Execute command file with scsi command trace
// --------------------------------------------

void ScsiDisk::executeTraceFile(FILE *commandfile, FILE *out=stdout){
  bool verbose=FALSE;
  char buf[21];
  int i;
  static int MAX=100;
  long long t0, t1, t2;
  float idle_time;
  ReadXCmd rc(0,20000);
  WriteXCmd wc(0,20000,NULL);
  SeekXCmd sc(0);
  int start, len;
  int rem_start, rem_size, max_size;
  int disk_cache;
  int n, r0,r1,r2;

  // Reg[0] is always 0 ! 
  long long Reg[MAX];
  for(i=0;i<MAX;i++) Reg[i]=0;


  fprintf(stderr, 
    "Starting Trace execution...\n");


try{

  //Reg[1] value is set to experiment start time
  Reg[1]=t0=rdtsc();

  while(!feof(commandfile)){

    fscanf(commandfile,"%20s", buf);
    switch(buf[0]){
        case 'T':
          fscanf(commandfile, "%d", &n);
          if(n>=MAX || n<0){
          	fprintf(stderr, 
          	  "Error in <tracefile>: T registar has to be 1-%d.\n",MAX-1);
            exit(-1);
          }
          Reg[n]=rdtsc();
        break;

        case '+':
#ifdef DEBUGt
  fprintf(stderr, 
    "+\n"); 
#endif
          fscanf(commandfile, "%d %d %d", &r0, &r1, &r2);
          if(r0<0 || r0>=MAX || r1<0 || r1>=MAX || r2<0 || r2>=MAX){
          	fprintf(stderr, 
          	  "Error in <tracefile>: T registar has to be 0-%d.\n",MAX-1);
            exit(-1);          
          } else {
            if(r0==0) fprintf(out, "%f\n", ftime(Reg[r1]+Reg[r2], 0));
            else Reg[r0]=Reg[r1]+Reg[r2];
          }
        break;
        
        case '-':
#ifdef DEBUGt
  fprintf(stderr, 
    "-\n"); 
#endif
          fscanf(commandfile, "%d %d %d", &r0, &r1, &r2);
          if(r0<0 || r0>=MAX || r1<0 || r1>=MAX || r2<0 || r2>=MAX){
          	fprintf(stderr, 
          	  "Error in <tracefile>: T registar has to be 0-%d.\n",MAX-1);
            exit(-1);          
          } else {
            if(r0==0) fprintf(out, "%f\n", ftime(Reg[r1],Reg[r2]));
            else Reg[r0]=Reg[r1]-Reg[r2];
          }
        break;

        case '/':
#ifdef DEBUGt
  fprintf(stderr, 
    "-\n"); 
#endif
          fscanf(commandfile, "%d %d %d", &r0, &r1, &r2);
          if(r0<0 || r0>=MAX || r1<0 || r1>=MAX || r2==0){
          	fprintf(stderr, 
          	  "Error in <tracefile>: T registar has to be 0-%d.\n",MAX-1);
            exit(-1);          
          } else {
            if(r0==0) fprintf(out, "%f\n", ftime(Reg[r1],0)/r2 );
            else Reg[r0]=Reg[r1]/r2;
          }
        break;

        case 'P':
          fscanf(commandfile, "%d", &r0);
          if(r0<0 || r0>=MAX){
          	fprintf(stderr, 
          	  "Error in <tracefile>: T registar has to be 0-%d.\n",MAX-1);
            exit(-1);          
          } else {
            fprintf(out, "%f\n", ftime(Reg[r0],0) );
          }
        break;

        case 'B':
          fscanf(commandfile, "%d", &disk_cache);
          if(disk_cache & 0x01){
            setCache(0,TRUE);
            fprintf(out, "READ cache: ON, ");
          } else{
            setCache(0,FALSE);
            fprintf(out, "READ cache:OFF, ");
          }
          if(disk_cache & 0x02){
            setCache(1,TRUE);
            fprintf(out, "WRITE cache: ON, ");
          } else{
            setCache(1,FALSE);
            fprintf(out, "WRITE cache:OFF, ");
          }
          if(disk_cache & 0x04){
            setCache(2,TRUE);
            fprintf(out, "Prefetch: ON\n");
          } else{
            setCache(2,FALSE);          
            fprintf(out, "Prefetch:OFF\n");
          }
        break;

        case 'C':
          fprintf(out,"%s",&buf[1]);
          while(1){
            buf[0]=fgetc(commandfile);
            if(buf[0]=='\n' || feof(commandfile)) break;
            fprintf(out, "%c",buf[0]);
          };
          ungetc('\n',commandfile);
          fprintf(out, "\n");
        break;


// Set verbose bit
        case 'V':
#ifdef DEBUGt
  fprintf(stderr, 
    "V\n"); 
#endif
          fscanf(commandfile, "%d", &n);
          if(n==0) verbose=FALSE;
          else verbose=TRUE;
        break;

// -------------------
// Do one Read request
// -------------------
    	case 'R':

#ifdef DEBUGt
  fprintf(stderr, 
    "R\n"); 
#endif

        fscanf(commandfile, "%d %d", &start, &len);

        t1=rdtsc();
        max_size=SG_DEF_RESERVED_SIZE/BLOCK_SIZE-2;
        rem_start=start;
        rem_size=len;

        while(rem_size>0){
          if(rem_size>max_size){
            rc.SetParam(rem_start,max_size);
            handleCmd(&rc);
            
            rem_size-=max_size;
            rem_start+=max_size;
          } else{
            rc.SetParam(rem_start,rem_size);
            handleCmd(&rc);

            rem_size=0;
          }
        }
        t2=rdtsc();
        
        if(verbose) fprintf(out,"%s completed at %f, using %f\n", 
                      buf, ftime(t2,t0), ftime(t2,t1));

     	break;

// --------------------
// Do one write request
// --------------------
    	case 'W':
#ifdef DEBUGt
  fprintf(stderr, 
    "W\n"); 
#endif
        fscanf(commandfile, "%d %d", &start, &len);

        t1=rdtsc();

        max_size=SG_DEF_RESERVED_SIZE/BLOCK_SIZE-2;
        rem_start=start;
        rem_size=len;

        while(rem_size>0){
          if(rem_size>max_size){
            wc.SetParam(rem_start,max_size,NULL);
            handleCmd(&wc);
            
            rem_size-=max_size;
            rem_start+=max_size;
          } else{
            wc.SetParam(rem_start,rem_size,NULL);
            handleCmd(&wc);

            rem_size=0;
          }
        }


        t2=rdtsc();    	
        if(verbose) fprintf(out,"%s completed at %f, using %f\n", 
                      buf, ftime(t2,t0), ftime(t2,t1));
    	break;

// -------------------
// Do one seek request
// -------------------
      case 'S':
#ifdef DEBUGt
  fprintf(stderr, 
    "S\n"); 
#endif
        fscanf(commandfile, "%d", &start);
        sc.SetParam(start);
        t1=rdtsc();
        handleCmd(&sc);
        t2=rdtsc();    	
        if(verbose) fprintf(out,"%s completed at %f, using %f\n", 
                      buf, ftime(t2,t0), ftime(t2,t1));
      break;

// ------------------------
// Wait for %f microseconds
// ------------------------
      case 'I':
        fscanf(commandfile, "%f", &idle_time);
        t1=rdtsc();
        t2=t1;
        while(ftime(t2,t1) < idle_time) t2=rdtsc();
      break;

      default:
      break;
    }
    skip2nl(commandfile);
    buf[0]='\0';
  }
}catch (Exception *e) {
  fprintf(stderr, "%s\n", e->getMsg());
  delete e;
  return;
}

}

