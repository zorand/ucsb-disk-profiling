#include "scsibench.h"

// --------
// ScsiDisk
// --------

bool ScsiDisk::Init(const char *filename, bool nonBlocking, bool queuing) {
  
  if(!ScsiDevice::Init(filename,nonBlocking,queuing)) return FALSE;
            
  findRpm();
  fprintf(stderr,"\nRPM=%f  (Tr=%f us)\n",rpm,Tr);
  ReadCapacityCmd rc_cmd;
  handleCmd(&rc_cmd);
  logical_size = rc_cmd.getCapacity();
  block_size = rc_cmd.getBlockSize();
  
  RigidGeomModePage *rgmp=new RigidGeomModePage;
  ModeSenseCmd rg_cmd(rgmp, MS_CurrentValues);
  handleCmd(&rg_cmd);
  cyl_num = rgmp->getCylinders();
  track_per_cyl = rgmp->getHeads();
  rgmp->Print();

  fprintf(stderr,"ScsiDisk:\nlog_size=%u, block_size=%u\n", logical_size, block_size);
  fprintf(stderr,"number of cyl=%u, number of tracks per cyl=%u\n",cyl_num,track_per_cyl);

  if(track_per_cyl<1){
    track_per_cyl=1;
    cyl_num=DEFAULT_CYL_NUM;
  }
  cyl_info = new CylInfo[cyl_num](track_per_cyl);
  fprintf(stderr,"Size of CylInfo[%d]=%d\n",cyl_num,cyl_num*cyl_info[0].memory_size );

  return TRUE;
}

// ---------------------------------
// Find rotational speed - Empirical
// ---------------------------------
void ScsiDisk::findRpm(bool verbose=FALSE){
  long long time1,time2,time3,last_time;
  const int NUM=200;
  WriteXCmd wc(0,1,NULL,TRUE);
  
  time1=rdtsc();
  for(int i=0;i<NUM;i++){
    last_time=rdtsc();
    handleCmd(&wc);
    if(i==(NUM/2-1)) time2=rdtsc();
    if(verbose) printf("%d %f\n", i, ftime(rdtsc(),last_time) );
  }
  time3=rdtsc();
  Tr=(ftime(time1,time3) - ftime(time1,time2))/(NUM-NUM/2);
  rpm=1000000.*60./Tr;
  rpm=1000000.*60.*(NUM-NUM/2)/(ftime(time1,time3) - ftime(time1,time2) );
}

// --------------------------------
// returns max size of read request
// --------------------------------
int ScsiDisk::findReadMax(){
  int maxsize=10240;
  long long t1,t2;

  for(int i=1;i<maxsize;i++){
    ReadXCmd rc(0,i);
    try{
      t1=rdtsc();
      handleCmd(&rc);
      t2=rdtsc();
    }catch (Exception *e) {
      fprintf(stderr, "%s\n", e->getMsg());
      delete e;
      return i-1;
    }
    if(!rc.isOK()){ 
      return i-1;
    }
  }
  return -1;
}

// ----------------------------------------------
// whichCache : 0 = READ, 1 = WRITE, 2 = Prefetch
// ----------------------------------------------
void ScsiDisk::setCache(int whichCache, bool state) {
  ModeSenseCmd cmd1(new CachingModePage(), MS_CurrentValues);
  handleCmd(&cmd1);

  CachingModePage* cachepage = new CachingModePage();
  ModeSelectCmd cmd2(cachepage, FALSE);

  cachepage->Set(cmd1.getPage());

  switch ( whichCache ) {
  case(0):
    cachepage->setReadCache(state);
    break;
  case(1):
    cachepage->setWriteCache(state);
    break;
  case(2):
    cachepage->setAbortPrefetch(!state);
    break;
  default:
    throw new Exception("invalid value whichCache (%d)", whichCache);
    break;
  }

  handleCmd(&cmd2);
}

bool ScsiDisk::getCache(int whichCache) {
  ModeSenseCmd cmd1(new CachingModePage(), MS_CurrentValues);
  handleCmd(&cmd1);

  CachingModePage* cachepage = (CachingModePage*)cmd1.getPage();

  switch ( whichCache ) {
  case(0):
    return cachepage->getReadCache();
    break;
  case(1):
    return cachepage->getWriteCache();
    break;
  case(2):
    return !cachepage->getAbortPrefetch();
    break;
  default:
    throw new Exception("invalid value whichCache (%d)", whichCache);
    break;
  }
}

// -------------------------------------------
// Ask disk about its geometry (Interrogative)
// -------------------------------------------
void ScsiDisk::findMappingCommand(bool verbose=FALSE){
  int track_size[track_per_cyl];
  int track_start[track_per_cyl];
  int track_num[track_per_cyl];
  
  TransAddrDiagPage *transpage = new TransAddrDiagPage();
  TransAddrDiagPage *diagresult = new TransAddrDiagPage();

  unsigned int nextlog;
  int i,t,j,k;

  if(verbose){
    printf("logical_size %d\n", logical_size);  
    printf("block_size %d\n", block_size);  
    printf("cyl_num %d\n", cyl_num);  
    printf("track_per_cyl %d\n", track_per_cyl);  
  }

  for(i=0; i<cyl_num; i++){
    cyl_info[i].bad=FALSE;
    for(t=0;t<track_per_cyl;t++){
      SendDiagCmd diagcmd01(transpage);
      transpage->setPhysical(i,t,0);
      ReceiveDiagCmd diagcmd02(diagresult);
      handleCmd(&diagcmd01);
      handleCmd(&diagcmd02);

      // starting logical block number for cylinder i
      // there is problem with this approach, which did not
      // exist in the findMappingCommand2: what if there is 
      // NO used block on c,t,0 physical block?

      if(!diagresult->isValidLogical()){
      // give the track one more chance!
        transpage->setPhysical(i,t,1);
        handleCmd(&diagcmd01);
        handleCmd(&diagcmd02);
        if(!diagresult->isValidLogical()){
          fprintf(stderr,"Hit non-logical sector at (%d,%d)!\n",i,t);
          cyl_info[i].bad=TRUE;
          cyl_info[i].trackbad[t]=TRUE;
          cyl_info[i].logstart[t]=LONG_MAX;
          continue;
        }
      }
      cyl_info[i].trackbad[t]=FALSE;
      nextlog=diagresult->getLogical();
      cyl_info[i].logstart[t]=nextlog;
    }
    unsigned int min_nextlog = cyl_info[i].logstart[0];
    for(j=1;j<track_per_cyl;j++){
      if( (min_nextlog>cyl_info[i].logstart[j]) && !cyl_info[i].trackbad[j])
        min_nextlog = cyl_info[i].logstart[j];
    }
    cyl_info[i].start=min_nextlog;

    if(i>0){
      unsigned int min_log = cyl_info[i-1].logstart[0];
      for(j=1;j<track_per_cyl;j++){
        if(min_log>cyl_info[i-1].logstart[j])
          min_log = cyl_info[i-1].logstart[j];
//        if(min_nextlog>cyl_info[i].logstart[j])
//          min_nextlog = cyl_info[i].logstart[j];
      }
      if(!cyl_info[i-1].bad && !cyl_info[i].bad)
        cyl_info[i-1].size = min_nextlog-min_log;
      else cyl_info[i-1].size=-1;

      for(j=0;j<track_per_cyl;j++){
        track_start[j]=cyl_info[i-1].logstart[j];
        track_num[j]=j;
      }
      
      // sort tracks by logical start:
      for(j=0;j<track_per_cyl-1;j++)
        for(k=j;k<track_per_cyl;k++)
          if(track_start[j]>track_start[k]){
            int temp_num; unsigned temp_start;
            
            temp_start = track_start[j];
            track_start[j]=track_start[k];
            track_start[k]=temp_start;
            
            temp_num = track_num[j];
            track_num[j]=track_num[k];
            track_num[k]=temp_num;
          }
      
//      for(j=0;j<track_per_cyl-1;j++) fprintf(stderr,"%d\n",track_start[j]);
          
      for(j=0;j<track_per_cyl-1;j++)
        track_size[j] = track_start[j+1]-track_start[j];
      track_size[j] = min_nextlog-track_start[j];

      if(verbose){
        printf("C %d %d %d ", i-1, 
                cyl_info[i-1].bad?(-1):(int)cyl_info[i-1].start, 
                cyl_info[i-1].bad?(-1):cyl_info[i-1].size);
        printf("T ");
        for(t=0;t<track_per_cyl-1;t++){
          printf("%d %d %d ",
            t, cyl_info[i-1].trackbad[t]?(-1):(int)cyl_info[i-1].logstart[t],
            cyl_info[i-1].trackbad[t]?
              (-1) :
              (int)(track_size[t]));
        }
        printf("%d %d %d\n",
          t,cyl_info[i-1].logstart[t],
          track_size[t]);
      }
    }
  }
}

void ScsiDisk::loadMapping(FILE *mapfile){
  unsigned int ls;
  int bs,cn,tpc;
  char buf[100];

  fscanf(mapfile,"%99s %d",buf,&ls);
  fscanf(mapfile,"%99s %d",buf,&bs);
  fscanf(mapfile,"%99s %d",buf,&cn);
  fscanf(mapfile,"%99s %d",buf,&tpc);

  cyl_num=cn;
  track_per_cyl=tpc;
  delete [] cyl_info;
  cyl_info = new CylInfo[cyl_num](track_per_cyl);

//  printf("%d %d %d %d\n",ls,bs,cn,tpc);
//  printf("Size of CylInfo[%d]=%d\n",cyl_num,cyl_num*cyl_info[0].memory_size );

  while(!feof(mapfile)){
    int i,k,cstart,csize, t,tsize,tstart;

    i=-1;cstart=-1;    
    fscanf(mapfile,"%s %d %d %d",buf,&i,&cstart,&csize);
    if(feof(mapfile)) break;

//    printf("\nC %d %d %d ",i,cstart,csize);
    if(i>=0 && i<cyl_num){
      cyl_info[i].start=cstart;
      cyl_info[i].size=csize;
      if(cstart==-1 || csize<=0) cyl_info[i].bad=TRUE;
      else cyl_info[i].bad=FALSE;
    }
    
    fscanf(mapfile,"%s",buf);
    for(k=0;k<track_per_cyl;k++){
      t=-1;
      fscanf(mapfile,"%d %d %d", &t,&tstart,&tsize);
//      printf("%d %d %d\n",t,tstart,tsize); 
      if(t<0 || t>=track_per_cyl) 
        fprintf(stderr, "Error in mapfile: track should be [0-%d]\n",track_per_cyl-1);
      else{
        cyl_info[i].logstart[t]=tstart;
        cyl_info[i].logsize[t]=tsize;
        if(tstart!=-1) cyl_info[i].trackbad[t]=FALSE;
      }
    }
  }  
}

void ScsiDisk::findMappingCommand2(){
  unsigned c0,t0;

  TransAddrDiagPage *transpage = new TransAddrDiagPage();
  TransAddrDiagPage *diagresult = new TransAddrDiagPage();

  unsigned long start=0, nextlog;
  int i;

  for(i=0; i<cyl_num; i++){
    c0=i;
    t0=0;
    SendDiagCmd diagcmd01(transpage);
    transpage->setPhysical(c0,t0,0);
    ReceiveDiagCmd diagcmd02(diagresult);
    handleCmd(&diagcmd01);
    handleCmd(&diagcmd02);

// logical block number for cylinder i
    start=diagresult->getLogical();


    transpage->setPhysical(c0+1,t0,0);
    handleCmd(&diagcmd01);
    handleCmd(&diagcmd02);
    nextlog = diagresult->getLogical();
    printf("\n-->l=%8lu - next=%8lu := c:%5u, t:%2u, s:%4u\n",start,nextlog,c0,t0,0);
    printf("size= %lu\n", nextlog-start);

/*
    factor=size;
    l+=factor;
//   printf("------\nstart=%8lu, log=%8lu, c0=%d\n",start,l,i);
    do{
      SendDiagCmd diagcmd1(transpage);
      transpage->setLogical( l );
      ReceiveDiagCmd diagcmd2(diagresult);
      handleCmd(&diagcmd1);
      handleCmd(&diagcmd2);
      c = diagresult->getCylinder();
      t = diagresult->getHead();
      s = diagresult->getSector();
      printf("\n-->l=%8lu := c:%5u, t:%2u, s:%4u\n",l,c,t,s);


      if((s==0) && ((c-c0)*track_per_cyl+t-t0 == 1)){
        size=l-start;
//
        printf("logical=%8lu := c:%5u, t:%2u, s:%4u\n",l,c,t,s);
//
        printf("size of track %lu : %4u :: " ,t,size);
        cyl_info[i].bad=FALSE;
        cyl_info[i].logstart[0]=start;
        cyl_info[i].size=size;
        break;
      } else if(s!=0) {
        if( (c-c0)*track_per_cyl+t-t0 == 0){
          l+=factor;
        }else {
          factor/2 ? factor=factor/=2 : factor=1;
          l-=factor;
        }
      } else {
        if((c-c0)*track_per_cyl+t-t0>1){
#ifdef DEBUG
          fprintf(stderr,"!!! skipping track\n");
#endif
          break;
        }
      }
    } while (1);
*/
  }
}

// ---------------------------------------------
// Before call to this you need to findMapping
// using findMappingEmp or findMappingCommand
// Use cyl_info and prints zones, and throughput
// ---------------------------------------------

void ScsiDisk::findZones(bool verbose=FALSE){
  int i;
  unsigned start;
  int zone_start=0,new_zone_start;
  int zone=0, last_zone=0, new_zone_cnt=0;
  ReadXCmd rc(0,1,TRUE);
  FILE *outfile=stderr;

  if(verbose) outfile=stdout;
  fprintf(stderr, "Finding zones...\n");

  int size=cyl_info[0].size, new_zone_size;
  for(i=1;i<cyl_num;i++){

    if( (abs(cyl_info[i].size-size)>2 && !cyl_info[i].bad && cyl_info[i].size>0) 
            || (i==cyl_num-1) ){

    if(zone!=last_zone || i==cyl_num-1){
      if(i==cyl_num-1){
        zone++;
        new_zone_start=cyl_num;
      }
      last_zone=zone;
      fprintf(outfile,"\nZone %3d [%5d-%5d] sector#: %d, per track:%d\n",
              zone,zone_start,new_zone_start-1,size, size/track_per_cyl);

// Find Throughput:
      start=cyl_info[zone_start].start;

      fprintf(outfile,"Throughput: %f MB/s\n",
             findThroughput(cyl_info[zone_start].start,20000));

      fprintf(outfile,"Theoretical Max (size of track/Tr): %f MB/s\n",
                     cyl_info[zone_start].size*block_size/Tr/track_per_cyl);

//Find skew factors
      unsigned s,e,f;
      float m1,m2,m3;
      findCylSkew(cyl_info[zone_start].logstart[0],
                           cyl_info[zone_start].size * 2,
                           &s,&e,&f,
                           &m1,&m2,&m3);

      fprintf(outfile,"Cylinder # %d, ", zone_start);
      fprintf(outfile,"logical # %u\n", cyl_info[zone_start].logstart[0]);
      
      fprintf(outfile,"Cylinder Skew: %f us\n",m1);
      fprintf(outfile,"Track Skew: %f us\n",m2);
      fprintf(outfile,"Track Skew: %f us\n",m3);
      fprintf(outfile,"Cylinder Skew in blocks: %f\n",cyl_info[zone_start].size*m1/Tr/track_per_cyl);
      fprintf(outfile,"Track Skew in blocks: %f\n",cyl_info[zone_start].size*m2/Tr/track_per_cyl);
      fprintf(outfile,"Track Skew in blocks: %f\n",cyl_info[zone_start].size*m3/Tr/track_per_cyl);
      fprintf(outfile,"-------------------\n");

      fprintf(stderr, "zone %d\n",zone);
      zone_start=new_zone_start;
      size=cyl_info[new_zone_start].size;
      new_zone_cnt=0;
      continue;
    } 
      
      if(new_zone_cnt<=0){
        new_zone_start=i;
        printf("new_zone_start=%d\n",new_zone_start);
        new_zone_size=cyl_info[i].size;
        new_zone_cnt=1;
      } else if(new_zone_cnt>10){ 
        zone++;
      } else{
        if(cyl_info[i].size==size) new_zone_cnt=0;
        else new_zone_cnt++;
      }
    }
  }
}     

void ScsiDisk::findZones2(bool verbose=FALSE){
  int i,j;
  long long t1,t2;
  int size=cyl_info[0].size;
  unsigned start;
  const int DEF_SIZE=100;
  int zone_start=0;
  int zone=0;
  ReadXCmd rc(0,1,TRUE);
  FILE *outfile=stderr;
  
  if(verbose) outfile=stdout;
  
  for(i=1;i<cyl_num;i++){
      if(cyl_info[i].size!=size && !cyl_info[i].bad && i!=(cyl_num-1)){
  	  fprintf(outfile,"\nZone %3d [%5d-%5d] sector#: %d\n",
  	          zone,zone_start,i-1,cyl_info[i].size);

// Find Throughput:
      start=cyl_info[zone_start].logstart[0];
      
      try{
        t1=rdtsc();
        for(j=0;j<1000;j++){
          rc.SetParam(start+j*DEF_SIZE,DEF_SIZE);
          handleCmd(&rc);
        } 
        t2=rdtsc();
        fprintf(outfile,"Throughput: %f MB/s  %f us\n",
               1.*DEF_SIZE*j*block_size/ftime(t1,t2), ftime(t1,t2) );

        fprintf(outfile,"Theoretical Max (size of track/Tr): %f MB/s\n",
                       cyl_info[zone_start].size*block_size/Tr/track_per_cyl);

      } catch (Exception *e) {
        fprintf(stderr, "%s\n", e->getMsg());
        delete e;
      }
/*
      unsigned s,e,f;
      float m1,m2,m3;

      findCylSkew(cyl_info[zone_start].logstart[0],
                           cyl_info[zone_start].size*(track_per_cyl+1),
                           &s,&e,&f,
                           &m1,&m2,&m3);
      fprintf(stderr,"Cylinder Skew: %f\n",cyl_info[zone_start].size*m1/Tr);
      fprintf(stderr,"Track Skew: %f\n",cyl_info[zone_start].size*m2/Tr);
      fprintf(stderr,"Track Skew: %f\n",cyl_info[zone_start].size*m3/Tr);
*/
      zone++;
      zone_start=i;
      size=cyl_info[i].size;
    }
  }
}

// -------------------------------------------
// Find Throughput if you do not know mappings
// -------------------------------------------
float ScsiDisk::findThroughput(int thr_start, int thr_size){
  long long t1,t2;
  unsigned int start;
  int size;
  const int DEF_SIZE=2000;
  ReadXCmd rc(0,1,TRUE);

  start=thr_start;
  try{
    t1=rdtsc();
    while(start<(unsigned)(thr_start+thr_size) && start<logical_size){
      if(thr_start+thr_size - (int)start > DEF_SIZE) size=DEF_SIZE;
      else size = thr_start+thr_size - start;

      rc.SetParam(start,DEF_SIZE);
      handleCmd(&rc);
      start+=size;
    } 
    t2=rdtsc();
    return 1.*(start-thr_start)*block_size/ftime(t1,t2);
  }catch (Exception *e) {
    fprintf(stderr, "%s\n", e->getMsg());
    delete e;
    return -1;
  }
}

// ---------------------------
// Find Prefetch size method 1
// ---------------------------
int ScsiDisk::findPrefetchSize(float *hit_time,int start_block,bool verbose=FALSE){
  long long t1,t2;
  float time,mintime;
  int i,j;
  const int REPEAT=1;
  const int DEF_MAX_SIZE=5000;
  int min;
  ReadXCmd rc1(0,1);
  ReadXCmd rc2(1,1);
//  ReadXCmd readfar(logical_size-10,1,TRUE);


  if(verbose) fprintf(stderr,"Find Prefetch Size:\n---------------------------\n");

  for(i=1;i<=64;i++){
    rc1.SetParam(12345*i,1);
    handleCmd(&rc1);
    rc1.SetParam(12345*i+1,1);
    handleCmd(&rc1);
    rc1.SetParam(12345*i+2,1);
    handleCmd(&rc1);

    usleep(50000);
//    handleCmd(&readfar);

  }
  
  min=DEF_MAX_SIZE;
  for(i=0;i<REPEAT;i++){
    rc1.SetParam(start_block,1);
    handleCmd(&rc1);
//    rc1.SetParam(start_block+1,1);
//    handleCmd(&rc1);
    usleep(50000);
    mintime=Tr/4.;
    for(j=2;j<DEF_MAX_SIZE;j++){
      rc2.SetParam(start_block+j,1);
      t1=rdtsc();
      handleCmd(&rc2);
      t2=rdtsc();
      time=ftime(t1,t2);

      if(time<mintime) mintime=time;

      if(time>5*mintime){
        if(j-1<min){
          if(min!=DEF_MAX_SIZE) fprintf(stderr,"sumljivo :)\n"); 
          min=j-1;
        }
        *hit_time=mintime;
#ifdef DEBUG
        fprintf(stderr,"Prefetch size=%d\n",j-2);
        fprintf(stderr,"Prefetch mintime=%f\n",mintime);
#endif
        if(!verbose)break;
      }
      if(verbose) printf("%d %f\n",j,time);
    }
  }
  return min;
}

// ---------------------------
// Find Prefetch size method 2
// ---------------------------
bool ScsiDisk::findPrefetchSize2(int size, float time_th){
  long long t1,t2;
  float time;
  ReadXCmd rc1(5000,1);
  ReadXCmd rc2(5000+size+1,1);
  
  handleCmd(&rc1);
  usleep(50000);
  t1=rdtsc();
  handleCmd(&rc2);
  t2=rdtsc();
  time=ftime(t1,t2);
  if(time>time_th) return TRUE;
  else return FALSE;
}

// -------------------------------
// Find the number of cache blocks
// -------------------------------
int ScsiDisk::findCacheBlocksNumber(int pre_size, float mintime){
  long long t1,t2;
  float time;
  int i,j;
  const int NUMBER=256;
  ReadXCmd rc1(0,1);
  ReadXCmd rc2(1,1);
  
  for(i=1;i<NUMBER;i++){
#ifdef DEBUG
    fprintf(stderr,"---> i=%d\n",i);
#endif
    for(j=0;j<i;j++){
      rc1.SetParam(j*10000,1);
      handleCmd(&rc1);
      usleep(50000);
    }
    for(j=0;j<i;j++){
      rc2.SetParam(j*10000+pre_size-2,1);
      t1=rdtsc();
      handleCmd(&rc2);
      t2=rdtsc();
      time=ftime(t1,t2);
      if(time>mintime){
        fprintf(stderr,"Number of cache blocks=%d\n",i-1);
        return i-1;
      }else{ 
#ifdef DEBUG
      fprintf(stderr,"j=%d\n",j);
#endif
      }
    }
  }
  return i-1;
}

// ----------------------------------------
// Find the read cache replacement policy
// returns 1-possible FIFO, 2-possible LRU
// ----------------------------------------
int ScsiDisk::findCacheReplacePol(int pre_size, int cache_size, float mintime){
  long long t1,t2;
  float time;
  int j;
  ReadXCmd rc1(0,1);
  ReadXCmd rc2(1,1);
  int detect=0;
  
  // Fill read cache:
  for(j=0;j<cache_size;j++){
    rc1.SetParam(j*10000,1);
    handleCmd(&rc1);
    usleep(50000);
  }

  // Read something from first cache block
  rc2.SetParam(1,1);
  handleCmd(&rc2);

  // Read from new block
  rc2.SetParam(cache_size*10000,1);
  handleCmd(&rc2);
  usleep(50000);

  // Read first block again:
  rc2.SetParam(0,2);
  t1=rdtsc();
  handleCmd(&rc2);
  t2=rdtsc();
  time=ftime(t1,t2);
  fprintf(stderr,"time=%f\n",time);
  
  if(time>mintime){
    fprintf(stderr,"Looks like FIFO :)\n");
    detect=1;
  } else{
    fprintf(stderr,"Not FIFO, Assuming LRU\n");
  }

// Check if LRU
  
  // Fill read cache:
  for(j=cache_size+2;j<2*cache_size+2;j++){
    rc1.SetParam(j*10000,1);
    handleCmd(&rc1);
    usleep(50000);
  }

  // Read something from first cache block
  rc2.SetParam((cache_size+2)*10000+1,1);
  handleCmd(&rc2);

  // Read from new block
  rc2.SetParam(3*cache_size*10000,1);
  handleCmd(&rc2);
  usleep(50000);

  // Read from LRU block again:
  rc2.SetParam((cache_size+3)*10000+1,2);
  t1=rdtsc();
  handleCmd(&rc2);
  t2=rdtsc();
  time=ftime(t1,t2);

  fprintf(stderr,"time=%f\n",time);
  if(time>mintime){
    fprintf(stderr,"Looks like LRU\n");
    detect=2;
  } else{
    fprintf(stderr,"Not LRU...\n");
  }
  return detect;
}

// -----------------------------------------------
// Find if descard after read
// 0 - allways;  positive - stops after i 
// -----------------------------------------------
int ScsiDisk::findDiscardRead(int pre_size, int cache_size, float mintime){
  long long t1,t2;
  float time;
  int i,j;
  ReadXCmd rc1(0,1);
  ReadXCmd rc2(1,1);
  int DEF_MAX=5;
  
  // Fill read cache:
  for(j=cache_size+2;j<4*cache_size+2;j++){
    rc1.SetParam(j*10000,1);
    handleCmd(&rc1);
    usleep(50000);
  }

  for(i=0;i<DEF_MAX;i++){
    rc1.SetParam(0,1);
    t1=rdtsc();
    handleCmd(&rc1);
    usleep(50000);
    t2=rdtsc();
    time=ftime(t1,t2);
#ifdef DEBUG
    fprintf(stderr,"time=%f\n",time);
#endif
    if(time<=mintime){
      if(i==1) fprintf(stderr,"Does not discard block after read\n");
      else fprintf(stderr,"Discards block after read, but stops after %d reads\n",i);
      return i; 
    }
  }
  fprintf(stderr,"Allways discards block after read\n");
  return 0;
}

// -------------------------------
// Find the number of write blocks
// -------------------------------
int ScsiDisk::findWriteBufSize(float *hit_time, bool verbose=FALSE){
  long long t1,t2;
  float time,time0;
  int i,j;
  const int REPEAT=1;
  const int DEF_MAX_SIZE=2500;
  int min;
  WriteXCmd rc1(0,1,NULL);
  ReadXCmd readfar(logical_size-10,1,TRUE);
  
  min=DEF_MAX_SIZE;
  for(i=0;i<REPEAT;i++){
    time0=0;
    for(j=10;j<DEF_MAX_SIZE;j++){
// seek far
      handleCmd(&readfar);

      usleep(30000);
      rc1.SetParam(0,j,NULL);
      t1=rdtsc();
      handleCmd(&rc1);
      t2=rdtsc();
      time=ftime(t1,t2);
      if((time-time0>Tr/5) && (j>10) && (j<min)){
        if(min>j-1){
          min=j-1;
          *hit_time=time0;
          if(!verbose) break;
        }
      }
      if(verbose) printf("%d %f %f\n",j,time,time-time0);
      time0=time;
    }
  }
  return min;
}

// -------------------------------------------------
// Search for max in time diff from start
// -------------------------------------------------
float ScsiDisk::findCylSkew(unsigned start, unsigned number,
                            unsigned *pocetak, unsigned *kraj,unsigned *first,
                            float *m1=NULL,float *m2=NULL,float *m3=NULL,
                            bool verbose=FALSE){
  bool fua=TRUE;
  int step=1;

  unsigned int i;
  long long t1,t2,t3,to1=0,to2=0,to3=0;
  bool second_measurement=FALSE;
  float diff, old_diff;
  float time1, time2, diff1;
  float maxdiff1, maxdiff2, maxdiff3;
  float sum_maxdiff1=0.,sum_maxdiff2=0.,sum_maxdiff3=0.;
  int cnt_maxdiff1=0,cnt_maxdiff2=0,cnt_maxdiff3=0;
  long prev;
  ReadXCmd rc(start,1,fua);

  *first=0;

  maxdiff1=0.;  
  maxdiff2=0.;
  maxdiff3=0.;
  prev=start;
  for(i=start;i<start+number;i+=step){

    if(second_measurement==FALSE){
      to1=t1;
      to2=t2;
      to3=t3;
    }

    t1=rdtsc();
    rc.SetParam(start,1,fua);
    handleCmd(&rc);
    t2=rdtsc();
    rc.SetParam(i,1,fua);
    handleCmd(&rc);
    t3=rdtsc();
    time2=ftime(t2,t1);
    time1=ftime(t3,t2);
    diff1=ftime(t3,t2)-ftime(to3,to2);

    diff=diff1;

    
    if(i>start && fabs(diff) > 0.02*Tr){
      if(second_measurement==FALSE){
        second_measurement=TRUE;
		old_diff=diff;
        i--;
//fprintf(stderr, "boundary at %ld, repeating: %f %f\n",i,old_diff,diff);
        continue;
      }else{
        if(fabs(old_diff-diff) > 0.05*fabs(old_diff)){
          old_diff=diff;
          i--;
//fprintf(stderr, "boundary at %ld, repeating: %f %f\n",i,old_diff,diff);
          continue;
        } else{
          second_measurement=FALSE;
//fprintf(stderr, "boundary at %ld, confirmed: %f %f\n",i,old_diff,diff);
        }
      }
    }	
    if(diff < -0.3*Tr) diff+=Tr;
    if(diff > 0.7*Tr) diff=0;

    if(diff<0.6*Tr){
      if( diff>0.95*maxdiff1 ){
        if(diff>1.05*maxdiff1){
          maxdiff3=maxdiff2;
          cnt_maxdiff3=cnt_maxdiff2;
          sum_maxdiff3=sum_maxdiff2;
          maxdiff2=maxdiff1;
          cnt_maxdiff2=cnt_maxdiff1;
          sum_maxdiff2=sum_maxdiff1;
          maxdiff1=diff;
          sum_maxdiff1=maxdiff1;
          cnt_maxdiff1=1;
          *pocetak=start;
          *kraj=i;
          *first=i;
        } else{
          sum_maxdiff1+=diff;
          if(diff>maxdiff1) maxdiff1=diff;
          cnt_maxdiff1++;
          *pocetak=*kraj;
          *kraj=i;
          if(maxdiff1<diff) maxdiff1=diff;
          //maxdiff1=sum_maxdiff1/cnt_maxdiff1;
        }
      } else if( diff>0.95*maxdiff2 ){
        if(diff>1.05*maxdiff2){
          maxdiff3=maxdiff2;
          cnt_maxdiff3=cnt_maxdiff2;
          sum_maxdiff3=sum_maxdiff2;

          maxdiff2=diff;
          sum_maxdiff2=maxdiff2;
          cnt_maxdiff1=1;
        } else{
          if(diff>maxdiff2) maxdiff2=diff;
          sum_maxdiff2+=diff;
          cnt_maxdiff2++;
          maxdiff2=sum_maxdiff2/cnt_maxdiff2;
        }
      } else if(diff>0.95*maxdiff3){
        if(diff>1.05*maxdiff3){
          maxdiff3=diff;
          sum_maxdiff3=maxdiff3;
          cnt_maxdiff3=1;
        } else{
          if(diff>maxdiff3) maxdiff3=diff;
          sum_maxdiff3+=diff;
          cnt_maxdiff3++;
          maxdiff3=sum_maxdiff3/cnt_maxdiff3;
        }
      }
    }
    if(verbose)
      if(i>start) printf("%u %f %f %f %f\n",i,time2, time1,diff1,diff);

#ifdef DEBUG
    if(diff>700.){
      fprintf(stderr,"New track: %ld, distance: %ld\n",i,i-prev);
      prev=i;
    }
#endif

  }
  fprintf(stderr, "maxdiff1=%f, %d times\n",maxdiff1,cnt_maxdiff1);
  if(m1) *m1=maxdiff1;
  fprintf(stderr, "maxdiff2=%f, %d times\n",maxdiff2,cnt_maxdiff2);
  if(m2) *m2=maxdiff2;
  fprintf(stderr, "maxdiff3=%f, %d times\n",maxdiff3,cnt_maxdiff3);
  if(m3) *m3=maxdiff3;
  return maxdiff1;
}

// -------------------------------------------------
// Sample experiment for printing graphs
// -------------------------------------------------
void ScsiDisk::findReadDiff(unsigned start, unsigned number, int step, 
                            bool verbose=FALSE){
  bool fua=TRUE;
  unsigned end_block=0;

  unsigned i;
  long long t1,t2,t3,to1=0,to2=0,to3=0;
  float diff;
  float time1, time2, diff1;
  unsigned prev;
  ReadXCmd rc(start,1,fua);


  if(step==0) return;
  else if(step>0) end_block=start+number;
  else{
    end_block=start;
    start=start+number;
  }
  
  prev=start;
  for(i=start;step>0?i<end_block:i>end_block;i+=step){

    to1=t1;
    to2=t2;
    to3=t3;

    t1=rdtsc();
    rc.SetParam(start,1,fua);
    handleCmd(&rc);
    t2=rdtsc();
    rc.SetParam(i,1,fua);
    handleCmd(&rc);
    t3=rdtsc();
    time2=ftime(t2,t1);
    time1=ftime(t3,t2);
    diff1=ftime(t3,t2)-ftime(to3,to2);

    diff=diff1;
    
    if(diff < -0.3*Tr) diff+=Tr;
    if(diff > 0.7*Tr) diff=0;

    if(verbose)
      if(i>start) printf("%u %f %f %f %f\n",i,time2, time1,diff1,diff);

  }
}

// -----------------------------------------------------
// Empirically Find Cyl start and size for each cylinder
// -----------------------------------------------------

void ScsiDisk::findMappingEmp(){
  unsigned int start=0;
  unsigned int number=5000;
  unsigned s,e,f;
  int cyl_cnt=0;
  unsigned current_size=1;
  float cyl_skew;
  bool fua=TRUE;
  long long t1,t2,t3,to1=0,to2=0,to3=0;
  float time1, time2, diff;
  ReadXCmd rc(start,1,fua);

  delete cyl_info;
  cyl_info=new CylInfo[DEFAULT_CYL_NUM](1);
  cyl_num=DEFAULT_CYL_NUM;

  cyl_skew=findCylSkew(start,number,&s,&e,&f);
  fprintf(stderr, "CylSkew= %f, start=%u, end=%u, size=%u\n",cyl_skew,s,e,e-s);

  current_size=e-s;
  number=3*current_size+1;
  cyl_info[cyl_cnt].bad=false;
  cyl_info[cyl_cnt].logstart[0]=0;
  cyl_info[cyl_cnt].start=0;
  cyl_info[cyl_cnt].size=current_size;
  cyl_info[cyl_cnt].skew[0]=cyl_skew;
  
  while(cyl_cnt<cyl_num){
    if(cyl_info[cyl_cnt].logstart[0]+current_size >= logical_size) break;

    to1=rdtsc();
    rc.SetParam(cyl_info[cyl_cnt].logstart[0],1,fua);
    handleCmd(&rc);
    to2=rdtsc();
    rc.SetParam(cyl_info[cyl_cnt].logstart[0]+current_size-1,1,fua);
    handleCmd(&rc);
    to3=rdtsc();
  
    t1=rdtsc();
    rc.SetParam(cyl_info[cyl_cnt].logstart[0],1,fua);
    handleCmd(&rc);
    t2=rdtsc();
    rc.SetParam(cyl_info[cyl_cnt].logstart[0]+current_size,1,fua);
    handleCmd(&rc);
    t3=rdtsc();
    time2=ftime(t2,t1);
    time1=ftime(t3,t2);
    diff=ftime(t3,t2)-ftime(to3,to2);
    if(diff < -0.3*Tr) diff+=Tr;
    if(diff > 0.7*Tr) diff=0;
    if(diff > 0.9*cyl_skew){
      cyl_cnt++;
      cyl_info[cyl_cnt].logstart[0]=cyl_info[cyl_cnt-1].logstart[0]+current_size;
      cyl_info[cyl_cnt].start=cyl_info[cyl_cnt].logstart[0];
      cyl_info[cyl_cnt].bad=false;
      cyl_info[cyl_cnt].size=current_size;
      cyl_info[cyl_cnt].skew[0]=cyl_skew;
      fprintf(stderr,"cyl %u:skew %f, size %d\n",cyl_cnt,cyl_skew,current_size); 
    }else {
      start=cyl_info[cyl_cnt].logstart[0];
      if(start+number >= logical_size) break;
      while(1){
        cyl_skew=findCylSkew(start+current_size*3/4,number,&s,&e,&f);
        if((s==start) || (e-s < current_size/2)){
          start+=current_size;
          cyl_cnt++;
          if(cyl_cnt>=cyl_num){ cyl_cnt=cyl_num; break;}
          number*=2;
        }else{
          current_size=e-s;
          number=2*(e-s)+1;
          if(cyl_cnt+2<cyl_num) cyl_cnt+=2;
          else{cyl_cnt=cyl_num; break; }
          cyl_info[cyl_cnt].logstart[0]=s;
          cyl_info[cyl_cnt].bad=false;
          cyl_info[cyl_cnt].size=current_size;
          cyl_info[cyl_cnt].skew[0]=cyl_skew;
          fprintf(stderr,"cyl %u:skew %f, size %d\n",cyl_cnt,cyl_skew,current_size);
          break;
        }
      }
    }
  } 
  cyl_num=cyl_cnt;
}

// ----------------------------
// MTBRC
// ----------------------------
float ScsiDisk::MTBRCwr(unsigned start, unsigned end1, unsigned end2){
  float mintime;
  float time1,time2;
  long long t1,t2,t3;
  unsigned pos0,pos1,pos2;
  ReadXCmd rc(0,1,TRUE);
  WriteXCmd wc(0,1,NULL,TRUE);

  mintime=9.e+30;
  pos0=end1;
  pos1=(end1+end2)/2;
  pos2=end2;

  wc.SetParam(start,1,NULL,TRUE);
  t1=rdtsc();
  handleCmd(&rc);
  rc.SetParam(pos1,1,TRUE);
  t2=rdtsc();
  handleCmd(&rc);
  t3=rdtsc();
  time1=ftime(t3,t2);

  wc.SetParam(start,1,NULL,TRUE);
  t1=rdtsc();
  handleCmd(&wc);
  rc.SetParam(pos2,1,TRUE);
  t2=rdtsc();
  handleCmd(&rc);
  t3=rdtsc();
  time2=ftime(t3,t2);

  for(int j=0;(j<20) && (pos1!=pos2) && (pos0!=pos1);j++){
    if(time1<time2){
      mintime=time1;
      pos2=pos1;
      pos1=pos0+(pos1-pos0)/2;
      time2=time1;
    } else {
      mintime=time2;
      pos0=pos1;
      pos1=pos1+(pos2-pos1)/2;
    }
    wc.SetParam(start,1,NULL,TRUE);
    t1=rdtsc();
    handleCmd(&wc);
      
    rc.SetParam(pos1,1,TRUE);
    t2=rdtsc();
    handleCmd(&rc);
    t3=rdtsc();
    time1=ftime(t3,t2);
//      fprintf(stderr,"pos0=%3u,pos1=%3u,pos2=%3u,time1=%10f,time2=%10f\n",
//              pos0,pos1,pos2,time1,time2);
  }
  return mintime;
}

// ----------------------------
// MTBRC
// ----------------------------
float ScsiDisk::MTBRCrw(unsigned start, unsigned end1, unsigned end2,bool set=FALSE){
  float mintime;
  float time1,time2;
  long long t1,t2,t3;
  unsigned pos0,pos1,pos2;
  ReadXCmd rc(0,1,TRUE);
  WriteXCmd wc(0,1,NULL,TRUE);

  mintime=9.e+30;
  pos0=end1;
  pos1=(end1+end2)/2;
  pos2=end2;

// seek out of this cyl  
  if(set){
    rc.SetParam(start+10000,1,TRUE);
    handleCmd(&rc);
  }

  rc.SetParam(start,1,TRUE);
  t1=rdtsc();
  handleCmd(&rc);
  wc.SetParam(pos1,1,NULL,TRUE);
  t2=rdtsc();
  handleCmd(&wc);
  t3=rdtsc();
  time1=ftime(t3,t2);

  rc.SetParam(start,1,TRUE);
  t1=rdtsc();
  handleCmd(&rc);
  wc.SetParam(pos2,1,NULL,TRUE);
  t2=rdtsc();
  handleCmd(&wc);
  t3=rdtsc();
  time2=ftime(t3,t2);

  for(int j=0;(j<20) && (pos1!=pos2) && (pos0!=pos1);j++){
    if(time1<time2){
      mintime=time1;
      pos2=pos1;
      pos1=pos0+(pos1-pos0)/2;
      time2=time1;
    } else {
      mintime=time2;
      pos0=pos1;
      pos1=pos1+(pos2-pos1)/2;
    }
  // seek out of this cyl  
    if(set){
      rc.SetParam(start+10000,1,TRUE);
      handleCmd(&rc);
    }
    rc.SetParam(start,1,TRUE);
    t1=rdtsc();
    handleCmd(&rc);

    wc.SetParam(pos1,1,NULL,TRUE);
    t2=rdtsc();
    handleCmd(&wc);
    t3=rdtsc();
    time1=ftime(t3,t2);
//      fprintf(stderr,"pos0=%3u,pos1=%3u,pos2=%3u,time1=%10f,time2=%10f\n",
//              pos0,pos1,pos2,time1,time2);
  }
  return mintime;
}


// ----------------------------
// MTBRCrr
// ----------------------------
float ScsiDisk::MTBRCrr(unsigned start, unsigned end1, unsigned end2){
  float mintime;
  float time1,time2;
  long long t1,t2,t3;
  unsigned pos0,pos1,pos2;
  ReadXCmd rc(0,1,TRUE);

  mintime=9.e+30;
  pos0=end1;
  pos1=(end1+end2)/2;
  pos2=end2;

  rc.SetParam(start,1,TRUE);
  t1=rdtsc();
  handleCmd(&rc);
  rc.SetParam(pos1,1,TRUE);
  t2=rdtsc();
  handleCmd(&rc);
  t3=rdtsc();
  time1=ftime(t3,t2);

  rc.SetParam(start,1,TRUE);
  t1=rdtsc();
  handleCmd(&rc);
  rc.SetParam(pos2,1,TRUE);
  t2=rdtsc();
  handleCmd(&rc);
  t3=rdtsc();
  time2=ftime(t3,t2);

  for(int j=0;(j<20) && (pos1!=pos2) && (pos0!=pos1);j++){
    if(time1<time2){
      mintime=time1;
      pos2=pos1;
      pos1=pos0+(pos1-pos0)/2;
      time2=time1;
    } else {
      mintime=time2;
      pos0=pos1;
      pos1=pos1+(pos2-pos1)/2;
    }
    rc.SetParam(start,1,TRUE);
    t1=rdtsc();
    handleCmd(&rc);
      
    rc.SetParam(pos1,1,TRUE);
    t2=rdtsc();
    handleCmd(&rc);
    t3=rdtsc();
    time1=ftime(t3,t2);
//      fprintf(stderr,"pos0=%3u,pos1=%3u,pos2=%3u,time1=%10f,time2=%10f\n",
//              pos0,pos1,pos2,time1,time2);
  }
  return mintime;
}

// ----------------------------
// MTBRCww
// ----------------------------
float ScsiDisk::MTBRCww(unsigned start, unsigned end1, unsigned end2){
  float mintime;
  float time1,time2;
  long long t1,t2,t3;
  unsigned pos0,pos1,pos2;
  WriteXCmd wc(0,1,NULL,TRUE);

  mintime=9.e+30;
  pos0=end1;
  pos1=(end1+end2)/2;
  pos2=end2;

  wc.SetParam(start,1,NULL,TRUE);
  t1=rdtsc();
  handleCmd(&wc);
  wc.SetParam(pos1,1,NULL,TRUE);
  t2=rdtsc();
  handleCmd(&wc);
  t3=rdtsc();
  time1=ftime(t3,t2);

  wc.SetParam(start,1,NULL,TRUE);
  t1=rdtsc();
  handleCmd(&wc);
  wc.SetParam(pos2,1,NULL,TRUE);
  t2=rdtsc();
  handleCmd(&wc);
  t3=rdtsc();
  time2=ftime(t3,t2);

  for(int j=0;(j<20) && (pos1!=pos2) && (pos0!=pos1);j++){
    if(time1<time2){
      mintime=time1;
      pos2=pos1;
      pos1=pos0+(pos1-pos0)/2;
      time2=time1;
    } else {
      mintime=time2;
      pos0=pos1;
      pos1=pos1+(pos2-pos1)/2;
    }
    wc.SetParam(start,1,NULL,TRUE);
    t1=rdtsc();
    handleCmd(&wc);
      
    wc.SetParam(pos1,1,NULL,TRUE);
    t2=rdtsc();
    handleCmd(&wc);
    t3=rdtsc();
    time1=ftime(t3,t2);
//      fprintf(stderr,"pos0=%3u,pos1=%3u,pos2=%3u,time1=%10f,time2=%10f\n",
//              pos0,pos1,pos2,time1,time2);
  }
  return mintime;
}

float ScsiDisk::findHeadSwitch(unsigned cyl){
  float twr0,twr1;
  if(cyl_info[cyl].bad) return -1.;
  
  twr1=MTBRCwr(cyl_info[cyl].logstart[0]+1, 
              cyl_info[cyl].logstart[0]+cyl_info[cyl].size,
              cyl_info[cyl].logstart[0]+2*cyl_info[cyl].size-1);


  twr0=MTBRCwr(cyl_info[cyl].logstart[0]+1, 
                 cyl_info[cyl].logstart[0],
                 cyl_info[cyl].logstart[0]+cyl_info[cyl].size-1);

  return twr1-twr0;
}

float ScsiDisk::findWriteSet(unsigned cyl){
  float twr0,twr1;
  if(cyl_info[cyl].bad) return -1.;
  
  twr0=MTBRCrw(cyl_info[cyl].logstart[0]+1, 
              cyl_info[cyl].logstart[0],
              cyl_info[cyl].logstart[0]+cyl_info[cyl].size-1);

  twr1=MTBRCrw(cyl_info[cyl].logstart[0]+1, 
                 cyl_info[cyl].logstart[0],
                 cyl_info[cyl].logstart[0]+cyl_info[cyl].size-1,TRUE);

  return twr1-twr0;
}


// -----------------------------------------------------------------------
// Seek times: we need seek curve, complete seek curve, average seek times
// and logical number of block on destination cylinder with minimal access
// time (just seek time with small (or zero) rotational time
// -----------------------------------------------------------------------
unsigned int ScsiDisk::seekToMin(
                       unsigned start_log, int dest_cyl,
                       int dest_track){
  unsigned i=dest_cyl;
  unsigned dest_log;
  float mintime;
  float time1,time2;
  long long t1,t2,t3;
  unsigned start,pos0,pos1,pos2,size;
  ReadXCmd rc(0,1,TRUE);

  if(cyl_info[i].bad){
    fprintf(stderr,"%d is bad, skipping\n",i);
    return 0;
  }
  if(dest_track>=track_per_cyl){
    fprintf(stderr,"Destination track number too large\n");
  }
  if( start_log >= logical_size ){
    fprintf(stderr,"Start block number too large\n");
    return 0;
  } else{
    start=start_log;
  }
    
  mintime=9.e+30;
  size=cyl_info[i].size;
  pos0=0;
  pos1=size/2;
  pos2=size-1;
//  if(cyl_info[i].start+pos1 >= logical_size);

  t1=rdtsc();
  rc.SetParam(start,1,TRUE);
  handleCmd(&rc);
  rc.SetParam(cyl_info[i].logstart[dest_track]+pos1,1,TRUE);
  t2=rdtsc();
  handleCmd(&rc);
  t3=rdtsc();
  time1=ftime(t3,t2);

  t1=rdtsc();
  rc.SetParam(start,1,TRUE);
  handleCmd(&rc);
  rc.SetParam(cyl_info[i].logstart[dest_track]+pos2,1,TRUE);
  t2=rdtsc();
  handleCmd(&rc);
  t3=rdtsc();
  time2=ftime(t3,t2);

  for(int j=0;(j<20) && (pos1!=pos2) && (pos0!=pos1);j++){
    if(time1<time2){
      mintime=time1;
      pos2=pos1;
      pos1=pos0+(pos1-pos0)/2;
      time2=time1;
    } else {
      mintime=time2;
      pos0=pos1;
      pos1=pos1+(pos2-pos1)/2;
    }
    t1=rdtsc();
    rc.SetParam(start,1,TRUE);
    handleCmd(&rc);
      
    rc.SetParam(cyl_info[i].logstart[dest_track]+pos1,1,TRUE);
    t2=rdtsc();
    handleCmd(&rc);
    t3=rdtsc();
    time1=ftime(t3,t2);
//      fprintf(stderr,"pos0=%3u,pos1=%3u,pos2=%3u,time1=%10f,time2=%10f\n",
//              pos0,pos1,pos2,time1,time2);
  }
  dest_log=cyl_info[i].start+pos1;

  printf("start_log=%d, dest_cyl=%d, dest_track=%d: mintime=%f\n", 
      start_log, dest_cyl, dest_track, mintime);
  printf("dest: start=%d optimal=%d\n",cyl_info[dest_cyl].start,dest_log);  

  return dest_log;
}


// -------------------------------------------------------------------------
// Seek curve: prints seek curve from start_cyl to max distance (both sides) 
// and skiping every time prec cyls
// -------------------------------------------------------------------------
float ScsiDisk::seekCurve(int start_cyl, int maxdistance, int prec, int method=1){
  int i;
  float mintime;
  float time1,time2;
  float avg;
  unsigned cnt_avg;
  long long t1,t2,t3;
  unsigned start,pos0,pos1,pos2,size;
  ReadXCmd rc(0,1,TRUE);
  SeekXCmd sc(0);

  if(start_cyl>=cyl_num-1){
    fprintf(stderr, "Seek Curve start_cyl too large!\n");
    return -1.;
  } else if(cyl_info[start_cyl].bad){ 
    fprintf(stderr, "Seek Curve start_cyl marked bad!\n");
    return -2.;
  }
  else start=cyl_info[start_cyl].start;

  fprintf(stderr,"Seek curve: starting cyl:%u cyl_num=%u\n",start_cyl,cyl_num);

  avg=0.;
  cnt_avg=0;
  for(i=0;i<cyl_num;i+=prec){

    if(method==1){
      if(cyl_info[i].bad){
#ifdef DEBUG
        fprintf(stderr,"%d is bad, skipping\n",i);
#endif
        continue;
      }
      if((start_cyl>i?start_cyl-i:i-start_cyl)>maxdistance) continue;
    
      mintime=9.e+30;
      size=cyl_info[i].size/track_per_cyl;
      pos0=0;
      pos1=size/2;
      pos2=size-1;
      if(cyl_info[i].start+pos1 >= logical_size) break;

      t1=rdtsc();
      rc.SetParam(start,1,TRUE);
      handleCmd(&rc);
      t2=rdtsc();
      rc.SetParam(cyl_info[i].start+pos1,1,TRUE);
      handleCmd(&rc);
      t3=rdtsc();
      time1=ftime(t3,t2);

      t1=rdtsc();
      rc.SetParam(start,1,TRUE);
      handleCmd(&rc);
      rc.SetParam(cyl_info[i].start+pos2,1,TRUE);
      t2=rdtsc();
      handleCmd(&rc);
      t3=rdtsc();
      time2=ftime(t3,t2);

      for(int j=0;(j<20) && (pos1!=pos2) && (pos0!=pos1);j++){
        if(time1<time2){
          mintime=time1;
          pos2=pos1;
          pos1=pos0+(pos1-pos0)/2;
          time2=time1;
        } else {
          mintime=time2;
          pos0=pos1;
          pos1=pos1+(pos2-pos1)/2;
        }
        t1=rdtsc();
        rc.SetParam(start,1,TRUE);
        handleCmd(&rc);
      
        rc.SetParam(cyl_info[i].start+pos1,1,TRUE);
        t2=rdtsc();
        handleCmd(&rc);
        t3=rdtsc();
        time1=ftime(t3,t2);
//        fprintf(stderr,"pos0=%3u,pos1=%3u,pos2=%3u,time1=%10f,time2=%10f\n",
//                pos0,pos1,pos2,time1,time2);
      }
      avg+=mintime;
      cnt_avg++;
      printf("%d %f\n", i-start_cyl, mintime);  
    } else{
      if(!cyl_info[i].bad){
        sc.SetParam(cyl_info[i].start);
        t1=rdtsc();
        rc.SetParam(start,1,TRUE);
        handleCmd(&rc);
        t2=rdtsc();
        handleCmd(&sc);
        t3=rdtsc();
        mintime=ftime(t3,t2);
        printf("%d %f\n", i-start_cyl, mintime);
        avg+=mintime;
        cnt_avg++;  
      }
    }
  }    
  avg/=cnt_avg;
  fprintf(stderr,"End seek curve %d, avg time=%f\n",start_cyl,avg);
  return avg;
}

float ScsiDisk::avgSeekTime(int step, int prec,int method=1){
  float avg_all;
  int cnt_all;
  int start_cyl;

  avg_all=0.;
  cnt_all=0;
  for(start_cyl=0;start_cyl<cyl_num;start_cyl+=step){
    avg_all+=seekCurve(start_cyl,cyl_num,prec,method);
    cnt_all++;
  }
  avg_all/=cnt_all;
  fprintf(stderr,"\n ---------- \n Avg seek time: %f\n\n",avg_all);

  for(start_cyl=0;start_cyl<cyl_num;start_cyl+=step){
    seekCurve(start_cyl,100,5,method);
  }

  return avg_all;
}


void ScsiDisk::printZoneThr(FILE *outfile, int sample_num, int size=10000){
  unsigned int start=0;
  unsigned int step=logical_size/sample_num;

  while(start < logical_size-size){
    fprintf(outfile,"%u %f\n", start, findThroughput(start,size));
    start+=step;  
  }
}

// -------------------------------------------------------------------------
// Rotational delay prediction
// -------------------------------------------------------------------------
void ScsiDisk::rotCurve(int size=0, bool verbose=FALSE, FILE *out=stdout){
  int i,t;
  float time1;
  long long t1,t2,t3;
  unsigned pos1,logpos1;
  ReadXCmd r0(0,1,TRUE);
  ReadXCmd rc(0,1,TRUE);

  if(size==0) size=cyl_num;
  fprintf(stderr, "Rotational delay modeling 0-%d\n",size);


  for(i=0;i<size;i++){

    if(cyl_info[i].bad){
#ifdef DEBUG
      fprintf(stderr,"%d is bad, skipping\n",i);
#endif
      continue;
    }

    for(t=0;t<track_per_cyl;t++){
      t1=rdtsc();
      handleCmd(&r0);
      rc.SetParam(cyl_info[i].logstart[t],1,TRUE);
      t2=rdtsc();
      handleCmd(&rc);
      t3=rdtsc();
      time1=ftime(t3,t2);

      while(time1>=Tr) time1-=Tr;
      pos1=cyl_info[i].size/track_per_cyl * (1. - time1/Tr);
      logpos1=cyl_info[i].logstart[t]+pos1;

      cyl_info[i].rot[t]=pos1;      
      if(verbose)
        fprintf(out,"%d %f %d %d\n", i*track_per_cyl+t, time1, pos1, logpos1);
    }
  }
}

// -------------------------------------------------------------------------
// Rotational delay prediction validation
// -------------------------------------------------------------------------
float ScsiDisk::rotDist(int x, int y, FILE *out=stdout){
  int i;
  const int REPEAT=10;
  float time1,min;
  long long t1,t2,t3;
  unsigned pos1;
  ReadXCmd r0(0,1,TRUE);
  ReadXCmd rc(0,1,TRUE);
  int x_c,x_t,x_start,x_skew;
  int y_c,y_t,y_start,y_skew;
  float X,Y,rot_dist,err1;

  TransAddrDiagPage *transpage = new TransAddrDiagPage();
  TransAddrDiagPage *diagresult = new TransAddrDiagPage();
  SendDiagCmd diagcmd01(transpage);
  ReceiveDiagCmd diagcmd02(diagresult);

  transpage->setLogical(x);   
  handleCmd(&diagcmd01);
  handleCmd(&diagcmd02);
  x_c=diagresult->getCylinder();
  x_t=diagresult->getHead();                                
  x_start=cyl_info[x_c].logstart[x_t];
  x_skew=cyl_info[x_c].rot[x_t];

  transpage->setLogical(y);   
  handleCmd(&diagcmd01);
  handleCmd(&diagcmd02);
  y_c=diagresult->getCylinder();
  y_t=diagresult->getHead();                                
  y_start=cyl_info[y_c].logstart[y_t];
  y_skew=cyl_info[y_c].rot[y_t];

  Y = 1.0 * (y-y_start-y_skew) / cyl_info[y_c].logsize[y_t];
  X = 1.0 * (x-x_start-x_skew) / cyl_info[x_c].logsize[x_t];
  if(Y<0) Y+=1.0;
  if(X<0) X+=1.0;
  rot_dist=(Y-X)*Tr;
  if(rot_dist<0) rot_dist+=Tr;

//  fprintf(out,"x=%d, x_start=%d, x_skew=%d x_size=%d, X=%f\n", 
//    x,x_start,x_skew,cyl_info[x_c].logsize[x_t], X);
//  fprintf(out,"y=%d, y_start=%d, y_skew=%d\n", y,y_start,y_skew);
//  fprintf(out,"rotDist(x,y)=%f\n",rot_dist);

  r0.SetParam(x,1,TRUE);
  rc.SetParam(y,1,TRUE);

  min=9.9E30;
  for(i=0;i<REPEAT;i++){
    t1=rdtsc();
    handleCmd(&r0);
    t2=rdtsc();
    handleCmd(&rc);
    t3=rdtsc();
    time1=ftime(t3,t2);
    if(min>time1) min=time1;
  }

  err1=min-rot_dist;
  while(err1>0.5*Tr) err1-=Tr;

  fprintf(out,"%d %d %f %f %f %f %f\n",
    x,y,rot_dist,min,min-rot_dist,err1,err1/rot_dist);
  return min;
}

// -------------------------------------------------------------------------
// Rotational delay prediction Error rate
// -------------------------------------------------------------------------
void ScsiDisk::rotError(int size, int n=500){
  int i;
  int last,x,y;
  float rnd;
  
  srand(time(0));
  
  last = cyl_info[size-1].logstart[track_per_cyl-1];
  if(last < 1) return;

  for(i=0;i<n;i++){
    rnd = ((float)rand()) / RAND_MAX;
    x = last*rnd;

    rnd = ((float)rand()) / RAND_MAX;
    y = last*rnd;

//    fprintf(stderr,"rotDist %d\n",i);
    rotDist(x,y);
  }
}

