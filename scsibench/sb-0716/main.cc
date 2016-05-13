#include "scsibench.h"

#include <stdlib.h>

///////////////////////////////////
/// main()
///////////////////////////////////


void printRequestTimes(ScsiCmdBuffer *buffer) {
  int num = buffer->getNumCmds();
  
  unsigned long long last_stop = 0;
  for (int i = 0; i < num; i++) {
    ScsiCmd *cmd = buffer->getCmd(i);
    scsi_cmd_info* info = cmd->getInfo();

    // delay


    cmd->Print();
    //    fprintf(stderr, "%s:\t%f us\n", 
    //	    cmd->getCmdName(),
    //	    ftime(info->stop_time,info->start_time));

    last_stop = info->stop_time;
  }
    
}

// ----------------------
// Command line agruments
// ----------------------
void dumpHelp(){
  fprintf(stderr, "Usage: scsibench [/dev/sg<disk>] -? -t -c -e -i -a\n");
  fprintf(stderr, "-? Inquiry command\n");

  fprintf(stderr, "-t <logicalstart> <size> Measure throughput\n");

  fprintf(stderr, "-c Detect cache related stuff\n");
  fprintf(stderr, "-p Detect if prefetch is turned off\n");
  fprintf(stderr, "-m Detect head switch time and write settle time\n");
  fprintf(stderr, "-w Detect write buffer lenght\n");
  fprintf(stderr, "-wv Print write buf graph to stdio\n");

  fprintf(stderr, "-i Interogative mapping - scsi 2 disks\n");
  fprintf(stderr, "-e Try to detect mapping - slow\n");
  fprintf(stderr, "-f <mapfile> load mapping from file\n");

  fprintf(stderr, "-z Print info for each zone (need -i, -f, or -e)\n");
  fprintf(stderr, "-Z <sample_num> [size] Print throughput graph\n");

  fprintf(stderr, "-S Use seek command for seeks - faster\n");
  fprintf(stderr, "-a [step] [prec] Measure seek curves and dump to stdio\n");
  fprintf(stderr, "-s <from cyl> <max distance> <precision in cyls>\n");
  fprintf(stderr, "To be able to measure seek curves you need mapping\n");

  fprintf(stderr, "-r [start] [size] [step] Print read graphs\n");
  fprintf(stderr, "-k [start] [size] Print skew detecting graphs\n");

  fprintf(stderr, "-R Print rotational delay prediction graph\n");

  fprintf(stderr, "-x <tracefile> Read commands from file and execute them\n");
}

// ----
// Main
// ----
int main(int argc, char **argv) {
  ScsiDisk dev;
  char *devname="/dev/sgb";
  int argpointer=1;
  bool mapping_done=FALSE;
  bool seek_curve=FALSE;
  int avg_step;
  int avg_prec;
  bool do_one_seek=FALSE;
  int seek_start;
  int seek_dist;
  int seek_prec;
  float min_time;
  int prefetch_size,prefetch_start,prefetch_verbose,read_cache_size;
  bool mtbr=FALSE;
  bool verbose_rot;
  bool dozones=FALSE;
  unsigned start_skew=0,size_skew=5000;
  int step_skew;
  int sample_num,size;
  int size_rot;
  int thr_start,thr_size;
  int i;
  FILE *commandfile, *mapfile;
  int method=1;

  fprintf(stderr,"SG_DEF_RESERVED_SIZE=%d\n\n",SG_DEF_RESERVED_SIZE);
  time_tsc_init();

  if((argc>=2) && (argv[1][0]!='-')){
    devname=argv[1];
    argpointer++;
  }

  bool result = dev.Init(devname, FALSE, FALSE);
  if ( ! result ) {
    fprintf(stderr, "Cannot open device %s\n", devname);    
    fprintf(stderr, "Invalid argument: %s\n",argv[argpointer]);
    dumpHelp();
    exit(1);
  }

  ModeSenseCmd cmd1(new FormatModePage(), MS_CurrentValues);
  dev.handleCmd(&cmd1);
  cmd1.Print();

  while(argpointer<argc){
    if(argv[argpointer][0]!='-'){
      dumpHelp();
    }

    InquiryCmd cmd0;
    switch(argv[argpointer][1]){
      case '?':
        fprintf(stderr,"\n\n---> Inquiry Command:\n\n");
        dev.handleCmd(&cmd0);   
        cmd0.Print();
      break;

      case 't':
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0]))
          thr_start=atoi(argv[++argpointer]);
        else thr_start=0;
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0]))
          thr_size=atoi(argv[++argpointer]);
        else thr_size=5000;

      // Find throughput starting from 0:
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);

        fprintf(stderr,"Throughput: %f MB/s\n", 
          dev.findThroughput(thr_start,thr_size));

        fprintf(stderr,"\n\n---> Cache/Prefetch/Write Info:\n\n");        
      break;

      case 'c':
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);
        if(argv[argpointer][2]=='v') prefetch_verbose=TRUE;
        else prefetch_verbose=FALSE;

        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0]))
          prefetch_start=atoi(argv[++argpointer]);
        else prefetch_start=0;

    // Find prefetch size in blocks:        
        prefetch_size=dev.findPrefetchSize(&min_time,prefetch_start,prefetch_verbose);
        fprintf(stderr,"Prefetch size= %d, min_time=%f\n",prefetch_size,min_time);

    // Check if it is correct:
        fprintf(stderr,"Prefetch size check:\n");
        if(dev.findPrefetchSize2(prefetch_size-1,2000)) 
          fprintf(stderr,"%d\n", prefetch_size-1);

        if(dev.findPrefetchSize2(prefetch_size+1,2000)) 
          fprintf(stderr,"%d\n", prefetch_size+1);
          
        if(dev.findPrefetchSize2(prefetch_size,2000))
          fprintf(stderr,"%d\n", prefetch_size);

    // Find cache size in prefetch blocks:
        read_cache_size=dev.findCacheBlocksNumber(prefetch_size, 3*min_time);

    // Find replacement policy:
        dev.findCacheReplacePol(prefetch_size, read_cache_size, 3*min_time);

    // Check if discards block after read:
        dev.findDiscardRead(prefetch_size, read_cache_size, 3*min_time);
      break;

      case 'p':
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,FALSE);

        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0]))
          prefetch_start=atoi(argv[++argpointer]);
        else prefetch_start=0;

    // Find prefetch size in blocks:        
        prefetch_size=dev.findPrefetchSize(&min_time,prefetch_start);
        fprintf(stderr,"Prefetch size if turned off= %d, min_time=%f\n",
                prefetch_size,min_time);

    // Find cache size in prefetch blocks:
//        read_cache_size=dev.findCacheBlocksNumber(prefetch_size, 3*min_time);

    // Find replacement policy:
//        dev.findCacheReplacePol(prefetch_size, read_cache_size, 3*min_time);

    // Check if discards block after read:
        dev.findDiscardRead(prefetch_size, read_cache_size, 3*min_time);
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);
      break;

      case 'm':
        mtbr=TRUE;
      break;
      
      case 'w':
        float trans_time;
        int write_buf_size;
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);
        
    // Find Write buffer size in blocks:
        write_buf_size=dev.findWriteBufSize(&trans_time,
                       argv[argpointer][2]=='v' ? TRUE : FALSE);
        fprintf(stderr,"Write Buf size = %d, Write max throughput: %f MB/s\n",
                write_buf_size,write_buf_size*dev.getBlockSize()/trans_time);
      break;
      
      case 'e':
        dev.setCache(0,FALSE);
        dev.setCache(1,FALSE);
        dev.setCache(2,FALSE);

        fprintf(stderr,"Mapping in process... Please wait...\n");
        dev.findMappingEmp();
        mapping_done=TRUE;
        
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);
      break;
      
      case 'i':
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);

        fprintf(stderr,"Mapping in process...\n");
        dev.findMappingCommand(argv[argpointer][2]=='v' ? TRUE : FALSE);
        mapping_done=TRUE;
      break;

      case 'f':
        if( argv[argpointer+1] ){
          mapfile=fopen(argv[++argpointer], "r");
          if(mapfile==NULL){
            fprintf(stderr, "Cannot open file %s\n", argv[argpointer]);
            exit(-1);
          }
          dev.loadMapping(mapfile);
          fclose(mapfile);
          mapping_done=TRUE;
        }
        else{ 
          dumpHelp();
        }
      
      break;

      case 'z':
        dozones=TRUE;
      break;

      case 'Z':
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0]))
          sample_num=atoi(argv[++argpointer]);
        else sample_num=50;
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0]))
          size=atoi(argv[++argpointer]);
        else size=4000;

        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);        
        dev.printZoneThr(stdout, sample_num, size);
      break;

      case 'S':
        method=2;
      break;
      
      case 'a':
        seek_curve=TRUE;
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0]))
          avg_step=atoi(argv[++argpointer]);
        else avg_step=500;
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0]))
          avg_prec=atoi(argv[++argpointer]);
        else avg_prec=50;
      break;
      
      case 's':
        do_one_seek=TRUE;
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0])) seek_start=atoi(argv[++argpointer]);
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0])) seek_dist=atoi(argv[++argpointer]);
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0])) seek_prec=atoi(argv[++argpointer]);
        else{
          do_one_seek=FALSE;
          dumpHelp();
        }
//        fprintf(stderr,"seek: %d %d %d\n\n",seek_start,seek_dist,seek_prec);
      break;

      case 'k':
        unsigned a,b,c;
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0])) start_skew=atoi(argv[++argpointer]);
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0])) size_skew=atoi(argv[++argpointer]);

        dev.setCache(0,FALSE);
        dev.setCache(1,FALSE);
        dev.setCache(2,FALSE);
        dev.findCylSkew(start_skew,size_skew,&a,&b,&c,0,0,0,TRUE);
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);
      break;

      case 'r':
        start_skew=0;
        size_skew=1000;
        step_skew=1;
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0])) start_skew=atoi(argv[++argpointer]);
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0])) size_skew=atoi(argv[++argpointer]);
        if( argv[argpointer+1] ) step_skew=atoi(argv[++argpointer]);
        
        dev.setCache(0,FALSE);
        dev.setCache(1,FALSE);
        dev.setCache(2,FALSE);
        dev.findReadDiff(start_skew,size_skew,step_skew,TRUE);
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);
      break;

      case 'R':
        if(argv[argpointer][2]=='v') verbose_rot=TRUE;
        else verbose_rot=FALSE;
        size_rot=0;
        if( argv[argpointer+1] && isdigit(argv[argpointer+1][0])) 
          size_rot=atoi(argv[++argpointer]);
        
        dev.setCache(0,FALSE);
        dev.setCache(1,FALSE);
        dev.setCache(2,FALSE);

        dev.rotCurve(size_rot, verbose_rot);

        dev.rotError(size_rot);

        fprintf(stderr,"Finished rotational measurements.\n");
        dev.setCache(0,TRUE);
        dev.setCache(1,TRUE);
        dev.setCache(2,TRUE);
      break;

      case 'x':
        dev.setCache(0,FALSE);
        dev.setCache(1,FALSE);
        dev.setCache(2,FALSE);

        if( argv[argpointer+1] ){
          commandfile=fopen(argv[++argpointer], "r");
          if(commandfile==NULL){
            fprintf(stderr, "Cannot open file %s\n", argv[argpointer]);
            exit(-1);
          }
          dev.executeTraceFile(commandfile);
        }
        else{ 
          dumpHelp();
        }

      break;

      default:
        dumpHelp();
      break;
    }
    argpointer++;
  }

  if(seek_curve){
    if(!mapping_done){ 
      fprintf(stderr,"For seek curves you need mapping first. Use -e or -i\n");
      exit(-1);
    }
    dev.setCache(0,FALSE);
    dev.setCache(1,FALSE);
    dev.setCache(2,FALSE);

    dev.avgSeekTime(avg_step,avg_prec,method);
  }

  if(do_one_seek){
    if(!mapping_done){
      fprintf(stderr,"For seek curves you need mapping first. Use -e or -i\n");
      exit(-1);
    }
    dev.setCache(0,FALSE);
    dev.setCache(1,FALSE);
    dev.setCache(2,FALSE);

    dev.seekCurve(seek_start,seek_dist,seek_prec,method);
  }

  if(mtbr){
    float mt;
    if(!mapping_done){
      fprintf(stderr,"For MTBR you need mapping first. Use -e or -i\n");
      exit(-1);
    }
    dev.setCache(0,FALSE);
    dev.setCache(1,FALSE);
    dev.setCache(2,FALSE);
    for(i=0;i<20;i++){
      mt=dev.findHeadSwitch(0);
      fprintf(stdout,"Head switch [0] = %f\n",mt);
      mt=dev.findWriteSet(0);
      fprintf(stdout,"Write Set [0] = %f\n",mt);
    }
  }

  if(dozones){
    if(!mapping_done){
      fprintf(stderr,"For MTBR you need mapping first. Use -e or -i\n");
      exit(-1);
    }
    dev.setCache(0,TRUE);
    dev.setCache(1,TRUE);
    dev.setCache(2,TRUE);      
    dev.findZones(TRUE);
  }

//  dev.seekToMin(10000, 1000,0);

  dev.setCache(0,TRUE);
  dev.setCache(1,TRUE);
  dev.setCache(2,TRUE);      

  fprintf(stderr, "\n\nRead cache %s; write cache %s; prefetch %s\nEnd.\n", 
	  TRUEFALSE(dev.getCache(0)), 
	  TRUEFALSE(dev.getCache(1)),
	  TRUEFALSE(dev.getCache(2)));

//  int readmax=dev.findReadMax();
//  fprintf(stderr,"Read Max Size in Blocks: %d\n",readmax);

}
