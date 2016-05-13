#include "scsibench.h"

///////////////////////////////////
/// ScsiCmdBuffer
///////////////////////////////////


ScsiCmdBuffer::ScsiCmdBuffer(int max_cmds) throw(Exception*) {
  numCmds = 0;
  maxCmds = 0;
  info = NULL;

  info = new (ScsiCmd*)[max_cmds];
  if ( info == NULL )
    throw new Exception("%s", strerror(errno));

  // clear it
  for (int i = 0; i < max_cmds; i++)
    info[i] = NULL;

  maxCmds = max_cmds;
}

ScsiCmdBuffer::~ScsiCmdBuffer() {

  if ( info ) {
    for (int i = 0; i < numCmds; i++)
      delete info[i];
    delete[] info;
  }

  numCmds = 0;
  info = NULL;
  maxCmds = 0;
}


bool ScsiCmdBuffer::AddCmd(ScsiCmd *cmd)
{  
  if ( cmd == NULL ) {
    fprintf(stderr, "error: command is NULL\n");
    return FALSE;
  }

  if ( numCmds >= maxCmds ) {
    fprintf(stderr, "error: scsi_buffer full\n");
    return FALSE;
  }

  info[numCmds++] = cmd;

  return TRUE;
}

int ScsiCmdBuffer::getNumCmds() {
  return numCmds;
}

ScsiCmd* ScsiCmdBuffer::getCmd(int i) {
  if ( i < 0 || i >= numCmds )
    return NULL;

  return info[i];
}

void ScsiCmdBuffer::Print() {
  for (int j = 0; j < numCmds; j++) 
    info[j]->Print();  
}


///////////////////////////////////
/// ScsiDevice
///////////////////////////////////

// add a little extra to the bit bucket, just in case
#ifdef SG_BIG_BUFF
unsigned char ScsiDevice::bitBucket[SG_BIG_BUFF + 256];
#else
unsigned char ScsiDevice::bitBucket[4096 + 256];
#endif


ScsiDevice::ScsiDevice() {
  fd = -1;
}

ScsiDevice::~ScsiDevice() {
  if ( fd >= 0 )
    close(fd);
}

bool ScsiDevice::Init(const char *filename, bool nonBlocking, bool queuing) {
  if (filename == NULL) {
    fprintf(stderr, "ScsiDevice::Init: NULL Pointer");
    return FALSE;
  }
  
  int flags = O_RDWR | O_EXCL;
  if (nonBlocking) flags |= O_NONBLOCK;

  fd = open(filename, flags);
  if ( fd < 0 ) {
    perror("ScsiDevice::Init: open");
    return FALSE;
  }

//  if ( queuing ) {
//    int queue_status = 1;
//    // turn on command queuing
//    int result = ioctl(fd, SG_SET_COMMAND_Q, &queue_status);
//    if ( result != 0 )
//      perror("ScsiDevice::Init: SG_SET_COMMAND_Q ioctl");
//    }

  return TRUE;
}


// process a complete SCSI cmd. Use the generic SCSI interface.

bool ScsiDevice::handleCmds(ScsiCmdBuffer *buf, bool exception)
  throw (Exception*) 
{
  int status = 0;
  int which;
  scsi_cmd_info *inf;

  for (which = 0; which < buf->getNumCmds(); which++)
    doInit( buf->getCmd(which)->getInfo() );

  for (int which = 0; which < buf->getNumCmds(); which++ ) {
    inf = buf->getCmd(which)->getInfo();

    status = doWrite( inf );
    if (status != 0 ) goto error;
    
    status = doRead( inf );
    if (status != 0 ) goto error;

    continue;
  error:
    if ( exception )
      throw new Exception("handleCmds: command %s failed", 
			  buf->getCmd(which)->getCmdName());
    else
      return FALSE;
  }
    
  return TRUE;
}

bool ScsiDevice::handleCmd(ScsiCmd *cmd, bool exception)  throw (Exception*) {
  int status = 0;
  
  scsi_cmd_info *inf = cmd->getInfo();

  
  doInit(inf);
  status = doWrite( inf );
  if (status != 0 ) goto error;

  status = doRead( inf );
  if (status != 0 ) goto error;
    
  return TRUE;

 error:
  if ( exception )
    throw new Exception("handleCmd: command %s failed", cmd->getCmdName());
  else
    return FALSE;

}


void ScsiDevice::doInit(scsi_cmd_info *inf) throw (Exception*) {
  ASSERT( inf != NULL );

  /* set command as not done */
  inf->done = FALSE;

  /* safety checks */
  ASSERT( inf->in_buffer != NULL && inf->result_buffer != NULL && 
	  inf->in_header != NULL && inf->result_header != NULL &&
  	  inf->cmd_len != 0 && inf->cmd != NULL);

#ifdef SG_BIG_BUFF
//  fprintf(stderr,"SG_BIG_BUFF=%d\n",SG_BIG_BUFF);
  if ( (SCSI_OFF + inf->cmd_len + inf->in_size) > SG_BIG_BUFF ||
      (SCSI_OFF + inf->result_size) > SG_BIG_BUFF*512)
  { 
    throw new Exception("buffer is larger than maximum size (%u)",SG_BIG_BUFF);
  }
#else
  if (SCSI_OFF + inf->cmd_len + inf->in_size > 4096 ||
      SCSI_OFF + inf->out_size > 4096)
    throw new Exception("buffer is larger than maximum size (%u)", 4096);
#endif

  /* generic SCSI device header construction */
  inf->in_header->Set(SCSI_OFF + inf->result_size, inf->cmd_len == 12, 0);
  inf->result_header->Clear();
}

int ScsiDevice::doRead(scsi_cmd_info *inf) {
  int size;
  int status;

  //  unsigned long long start_time = GETTIME();  

  /* retrieve result */
  size = SCSI_OFF + inf->result_size;

  // use the bitbucket, if needed
  if ( inf->result_data == NULL && inf->result_size > 0 ) {
    status = read( fd, bitBucket, size);
    if ( status > 0 ) memcpy(inf->result_buffer, bitBucket, SCSI_OFF);

  } else {
    status = read( fd, inf->result_buffer, size);
  }

  inf->stop_time = GETTIME();
  inf->done = TRUE;

  //  fprintf(stderr, "Read time %f\n", ftime(start_time, inf->stop_time));

  if ( status < 0 || status != size || inf->result_header->isOK() == FALSE)
    return 1;
  
  return 0;
}

int ScsiDevice::doWrite(scsi_cmd_info *inf) {
  int size;
  int status;
  inf->start_time = GETTIME();

  /* send command */
  size = SCSI_OFF + inf->cmd_len + inf->in_size;

  // use the bitbucket, if needed
  if ( inf->in_data == NULL && inf->in_size > 0 ) {
    memcpy(bitBucket, inf->in_buffer, SCSI_OFF + inf->cmd_len);
    status = write( fd, bitBucket, size);

  } else {
    status = write( fd, inf->in_buffer, size);
  }
  
  //  unsigned long long stop_time = GETTIME();  
  //  fprintf(stderr, "Write time %f\n", ftime(inf->start_time, stop_time));

  if ( status < 0 && errno == EDOM )
    return 2;

  if ( status < 0 || status != size || inf->in_header->isOK() == FALSE )
    return 1;

  return 0;
}

