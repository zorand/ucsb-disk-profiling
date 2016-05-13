#ifndef SCSIBENCH_H
#define SCSIBENCH_H

#include "basics.h"
#include "scsicmd.h"

// ---------------------------------------------------------------
// For scsi1 disks... We cannot get number of cylinders from disk
// This is just the quick hack to provide static max size. This is
// simple to change to dynamic, but we are not yet interested much 
// in these disks, and this works for now.
// ---------------------------------------------------------------
#define DEFAULT_CYL_NUM 20000


// ------------------------------------------------------------
// ScsiCmdBuffer is a list which holds ScsiCmds
// It can be used to execute a sequence of commands all at once
// ------------------------------------------------------------
class ScsiCmdBuffer {
 public:
  ScsiCmdBuffer(int max_cmds) throw(Exception*);
  ~ScsiCmdBuffer();

  bool AddCmd(ScsiCmd* cmd);

  int getNumCmds();
  ScsiCmd* getCmd(int i);

  void Print();

 private:
  int numCmds;
  int maxCmds;
  
  ScsiCmd** info;  
};

// ---------------------------------------------------------------------
// This is the Scsi Device, it has a method for executing Scsi commands.
// It can either take a single command, or a ScsiCmdBuffer.
// ---------------------------------------------------------------------
class ScsiDevice {
 public:
  ScsiDevice();
  ~ScsiDevice();

  bool Init(const char *filename, bool nonBlocking, bool queuing);
  
  // upon an error: if exception is TRUE, an Exception will be thrown,
  // otherwise FALSE will be returned.
  bool handleCmds(ScsiCmdBuffer *cmds, 
		  bool exception = FALSE) throw (Exception*);

  bool handleCmd(ScsiCmd *cmd, bool exception = FALSE) throw (Exception*);

 protected:
  void doInit(scsi_cmd_info *inf) throw (Exception*);
  int doWrite(scsi_cmd_info *inf);  // 0=OK, 1=FAIL, 2=RETRY
  int doRead(scsi_cmd_info *inf);   // 0=OK, 1=FAIL, 2=RETRY

  // this is for reads and writes which want to discard the data
  static unsigned char bitBucket[]; 

  int fd;
};

// -------------------------------------
// Class to store info for each cylinder
// -------------------------------------
class CylInfo {
 public:
  int memory_size;
  bool bad;
  bool *trackbad;
  float *skew;
  int *rot;
  int interleave;
  int size;
  unsigned int *logstart;
  int *logsize;
  unsigned int start;
  
  CylInfo(int n) { 
    skew = new float[n];
    rot = new int[n]; 
    logstart = new unsigned int[n]; 
    logsize = new int[n];
    trackbad = new bool[n];
    for(int i=0;i<n;i++) trackbad[i]=TRUE;
    bad=TRUE;
    memory_size=sizeof(class CylInfo) + 
               n*(sizeof(float)+2*sizeof(int)+sizeof(unsigned int)+sizeof(bool));
  }
  ~CylInfo(){
    delete [] skew;
    delete [] logstart;
    delete [] logsize;
    delete [] trackbad;
  }
};


// --------
// ScsiDisk
// --------
class ScsiDisk : public ScsiDevice {
 private:
  float rpm;
  float Tr;
  unsigned int logical_size;
  int block_size;
  int cyl_num;
  int track_per_cyl;
  CylInfo *cyl_info;
  
 public:
  ScsiDisk() : ScsiDevice() { };
  ~ScsiDisk() { };
  bool Init(const char *filename, bool nonBlocking, bool queuing);
  unsigned int getLogicalSize() { return logical_size; }
  unsigned int getBlockSize() { return block_size; }
  unsigned int getNumCyl() { return cyl_num; }
  unsigned int getNumTrack() { return track_per_cyl; }
  unsigned int getNumSector(unsigned int c) { return cyl_info[c].size; }
  float getRpm() { return rpm; }
  unsigned seekToMin(unsigned start_log, int dest_cyl, int dest_track);
  float seekCurve(int start, int maxdistance,int prec,int method=1);
  float avgSeekTime(int step,int prec,int method=1);

  // 0 = read cache, 1 = write cache
  void setCache(int whichCache, bool state);
  bool getCache(int whichCache);

  float findCylSkew(unsigned start,unsigned number,unsigned *,unsigned *,unsigned *,
                    float *m1=NULL, float *m2=NULL, float *m3=NULL,bool=FALSE);

  void findReadDiff(unsigned start, unsigned number, int step, 
                    bool verbose=FALSE);

  void findMappingEmp();
  void findMappingCommand(bool verbose=FALSE);
  void findMappingCommand2();
  void loadMapping(FILE *mapfile);

  void findRpm(bool verbose=FALSE);
  void findZones(bool=FALSE);
  void findZones2(bool=FALSE);
  void printZoneThr(FILE *outfile, int sample_num, int size=10000);

  int findReadMax();
  int findPrefetchSize(float *, int, bool = FALSE);
  bool findPrefetchSize2(int,float);
  int findCacheBlocksNumber(int pre_size, float mintime);
  int findWriteBufSize(float *hit_time,bool=FALSE);
  float findThroughput(int,int);
  int findCacheReplacePol(int pre_size, int cache_size, float mintime);
  int findDiscardRead(int pre_size, int cache_size, float mintime);
  float MTBRCwr(unsigned start_pos, unsigned end1, unsigned end2);
  float MTBRCrr(unsigned start_pos, unsigned end1, unsigned end2);
  float MTBRCww(unsigned start_pos, unsigned end1, unsigned end2);
  float MTBRCrw(unsigned start_pos, unsigned end1, unsigned end2,bool=FALSE);
  float findHeadSwitch(unsigned cyl);
  float findWriteSet(unsigned cyl);

  void rotCurve(int=0, bool=FALSE, FILE *out=stdout);
  float rotDist(int,int,FILE *out=stdout);
  void rotError(int,int=500);

  void executeTraceFile(FILE *commandfile, FILE *out=stdout);
};

#endif //SCSIBENCH_H
