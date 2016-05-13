/*
 * Revision 0.3 - 3/25/2001
 * Scsibench - David & Zoran
 */

#ifndef SCSICMD_H
#define SCSICMD_H

#include "basics.h"

// These should come from /usr/src/linux/scsi, since the versions
// in glibc-devel-2.1.2 (/usr/include/scsi) can be outdated.
extern "C" {
#include "include/scsi/scsi.h"
#include "include/scsi/sg.h"
}

// This is for maximum size of one scsi command read or write
// The compiler will complain, but it works - for now
//#define SG_DEF_RESERVED_SIZE 4194304
//#define SG_BIG_BUFF 4194304

// scsi offset
#define SCSI_OFF sizeof(struct sg_header)

/////////////////////////////////////////////////////////////////
// There are two main usages of ScsiCmds.
//
// First, as single commands:
//
//   ReadXCmd cmd(lba, sectors);
//   device.handleCmd(&cmd);
// 
// Second, using a buffer:
//
//   ScsiCmdBuffer buffer(10);
//   buffer.addCmd( new ReadXCmd(lba, sectors) );
//   ...
//   buffer.addCmd( new WriteXCmd(lba, sectors) );
//   device.handleCmds(&buffer);
//
// For commands which use pages (Send/Recv Diagnostics and Mode Sense/Select),
// you also have to deal with the page.  The general scheme is:
//   1. Create the page
//   2. Create the command, giving it the page
//   3. Set the page, using the Set* commands defined for that page
//
//   TransAddrDiagPage *transpage = new TransAddrDiagPage();
//   SendDiagCmd cmd1(transpage);
//   transpage->setLogical( 0xFFFF );
//   dev.handleCmd(&cmd1);
//
// For commands which use pages, but don't require you to set the page first
// (ie commands that read the page):
//
//   ModeSenseCmd cmd2(new CachingModePage(), MS_CurrentValues);
//   dev.handleCmd(&cmd2);
//
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
/// ScsiGenericHeader
/////////////////////////////////////////////////////////////////

/* These are values for the target_status field in sg_header */
enum TargetStatus {
  TargetStatus_Good                = 0x00,
  TargetStatus_CheckCondition      = 0x01,
  TargetStatus_ConditionGood       = 0x02,
  TargetStatus_Busy                = 0x04,
  TargetStatus_IntermediateGood    = 0x08,
  TargetStatus_IntermediateCondGood= 0x0a,
  TargetStatus_ReservationConflict = 0x0c,
  TargetStatus_CommandTerminated   = 0x11,
  TargetStatus_QueueFull           = 0x14
};

/* These are values for the host_status field in sg_header */
enum HostStatus {
  HostStatus_OK         = 0x00, // NO error
  HostStatus_NoConnect  = 0x01, // Couldn't connect before timeout period  
  HostStatus_BusBusy    = 0x02, // BUS stayed busy through time out period 
  HostStatus_TimeOut    = 0x03, // TIMED OUT for other reason              
  HostStatus_BadTarget  = 0x04, // BAD target, device not responding       
  HostStatus_Abort      = 0x05, // Told to abort for some other reason     
  HostStatus_Parity     = 0x06, // Parity error                            
  HostStatus_Error      = 0x07, // Internal error [DMA underrun on aic7xxx]
  HostStatus_Reset      = 0x08, // Reset by somebody.                      
  HostStatus_BadIntr    = 0x09, // Got an interrupt we weren't expecting.  
  HostStatus_PassThrough= 0x0a, // Force command past mid-layer            
  HostStatus_SoftError  = 0x0b  // The low level driver wants a retry     
};

/* These are values for the driver_status field in sg_header */
enum DriverStatus {
  DriverStatus_OK       =   0x00, // Typically no suggestion
  DriverStatus_Busy     =   0x01,
  DriverStatus_Soft     =   0x02,
  DriverStatus_Media    =   0x03,
  DriverStatus_Error    =   0x04,
  DriverStatus_Invalid  =   0x05,
  DriverStatus_Timeout  =   0x06,
  DriverStatus_Hard     =   0x07,
  DriverStatus_Sense    =   0x08 // Implies sense_buffer output
};
/* above status 'or'ed with one of the following suggestions */
enum DriverSuggest {
  DriverSuggest_Retry   =   0x10,
  DriverSuggest_Abort   =   0x20,
  DriverSuggest_Remap   =   0x30,
  DriverSuggest_Die     =   0x40,
  DriverSuggest_Sense   =   0x80
};

struct SenseData;

/*
 * ScsiGenericHeader is struct sg_header, plus some methods to
 * make dealing with it easier.
 */
typedef struct ScsiGenericHeader : sg_header {

  void Set(int replyLen, bool twelveByte, int packId);
  void Clear();

  bool isOK();

  TargetStatus targetStatus();

  HostStatus hostStatus();

  DriverStatus driverStatus();
  DriverSuggest driverSuggestion();

  SenseData* getSenseData();

  void Print();

} ScsiGenericHeader;

// sense keys
enum SenseKeys {
  SenseKey_NoSense           = 0x00,
  SenseKey_RecoveredError    = 0x01,
  SenseKey_NotReady          = 0x02,
  SenseKey_MediumError       = 0x03,
  SenseKey_HardwareError     = 0x04,
  SenseKey_IllegalRequest    = 0x05,
  SenseKey_UnitAttention     = 0x06,
  SenseKey_DataProtect       = 0x07,
  SenseKey_BlankCheck        = 0x08,
  SenseKey_CopyAborted       = 0x0a,
  SenseKey_AbortedCommand    = 0x0b,
  SenseKey_VolumeOverflow    = 0x0d,
  SenseKey_MisCompare        = 0x0e
};

enum SenseErrorCode {
  SenseErrorCode_Current,
  SenseErrorCode_Deferred,
  SenseErrorCode_Invalid
};

typedef struct SenseData {
  
  void Print();
  
  // is the sense data valid?
  bool valid();  

  SenseErrorCode errorCode();

  // Invalid Length Indicator -- block of data did not match the logical
  // block length of the data on the medium
  bool ILI();  

  SenseKeys senseKey();

  // for direct access devices, the LBA associated with the sense key
  unsigned informationLBA();

  unsigned char additionalSenseCode();
  unsigned char additionalSenseCodeQualifier();

  unsigned char data[SG_MAX_SENSE];
} SenseData;



/////////////////////////////////////////////////////////////////
/// scsi_cmd_info
/////////////////////////////////////////////////////////////////

/*
 * This gives an overview of the buffer layout in scsi_cmd_info.
 *
 * index_buffer:
 * +-------------------+
 * | ScsiGenericHeader | <- in_header (size = SCSI_OFF)
 * +-------------------+
 * | Scsi Command      | <- cmd       (size = cmd_len )
 * +-------------------+
 * | input data        | <- in_data   (size = in_size )
 * +-------------------+
 *
 * result_buffer:
 * +-------------------+
 * | ScsiGenericHeader | <- result_header (size = SCSI_OFF)
 * +-------------------+
 * | result data       | <- result_data   (size = result_size)
 * +-------------------+
 *
 * NOTE about in_data and result_data: 
 * - in_buffer and result_buffer are the input and result buffers, and are
 *   never NULL.
 * - in_header, cmd, in_data, result_header, and result_data are all pointers
 *   into the appropriate buffer, as shown above.  The headers and cmd are also
 *   never NULL.
 * - The in_bucket (or result_bucket) argument of the scsi_cmd_info constructor
 *   specifies that the input data (or result data) is not important and should
 *   be read from (or written to) the bit bucket.  In this case, buffer space 
 *   will NOT be allocated in in_buffer (or result_buffer), in_data (or
 *   result_data) will be set to NULL, and index_size (or result_size) will 
 *   still be set to the specified size.
 *   When the command is executed, the data must be read from and written to
 *   an intermediate buffer, but the returned data does not need to be 
 *   retained.
 * - in_data (or result_data) will also be set to NULL if in_size (or 
 *   result_size) is 0.
 */
class scsi_cmd_info {
 public:
  scsi_cmd_info(unsigned cmd_len, unsigned in_len, bool in_bucket, 
		unsigned result_len, bool result_bucket);
  ~scsi_cmd_info();

  void resize(unsigned cmd_len, unsigned in_len, bool in_bucket, 
	      unsigned result_len, bool result_bucket);

  unsigned cmd_len;         /* command length */

  unsigned char *in_buffer;     /* input buffer */
  unsigned in_buffer_size;      /* size of input buffer */
  ScsiGenericHeader* in_header; /* header index into buffer */
  unsigned char *cmd;           /* cmd index into buffer */
  unsigned char *in_data;       /* data index into buffer */
  unsigned in_size;             /* input data size */

  unsigned char *result_buffer;     /* result buffer */
  unsigned result_buffer_size;      /* size of result buffer */
  ScsiGenericHeader* result_header; /* header index into buffer */
  unsigned char* result_data;       /* data index into buffer */
  unsigned result_size;             /* result data size */

  unsigned long long start_time;
  unsigned long long stop_time;
  bool done;

 protected:
  void set(unsigned cmd_len, unsigned in_len, bool in_bucket,
	   unsigned result_len, bool result_bucket,
	   unsigned num_in_bytes, unsigned num_result_bytes) ;
};


/////////////////////////////////////////////////////////////////
/// ScsiCmd
/////////////////////////////////////////////////////////////////

// The base class to all Scsi Commands.
class ScsiCmd {
 public:
  ScsiCmd()  throw (Exception*) : my_info(NULL) {};
  virtual ~ScsiCmd();

  virtual void Print();

  scsi_cmd_info* getInfo();

  bool isDone();
  bool isOK();

  // returns the name of the scsi command
  virtual const char* getCmdName() = 0;

 protected:
  void __Print(); 

  scsi_cmd_info *my_info;
};


/////////////////////////////////////////////////////////////////
/// Various ScsiCmd implementations
/////////////////////////////////////////////////////////////////


// Inquiry command

#define INQUIRY_CMD     0x12
#define INQUIRY_CMDLEN  6
#define INQUIRY_REPLY_LEN 96
class InquiryCmd : public ScsiCmd {
 public:
  InquiryCmd() throw (Exception*);

  virtual void Print();

  const char* getVendor();
  const char* getProduct();
  const char* getProductRev();

  bool isDiskDevice();
  unsigned char ScsiRevision();

  virtual const char* getCmdName() { return "InquiryCmd"; };

 protected:
  char vendor_buf[9];
  char product_buf[17];
  char product_rev_buf[5];
};


// ReadX command

#define READ_EXT_CMD     0x28
#define READ_EXT_CMDLEN  10
class ReadXCmd : public ScsiCmd {
 public:
  // bitbucket specifies whether to discard read blocks, see scsi_cmd_buffer
  ReadXCmd(unsigned address, unsigned sectors, 
	   bool FUA = FALSE, bool bitbucket = TRUE) throw (Exception*);
  void SetParam(unsigned address, unsigned sectors, 
		bool FUA = FALSE, bool bitbucket = TRUE) throw (Exception*);

  virtual void Print();

  virtual const char* getCmdName() { return "ReadXCmd";};
};


// WriteX command

#define WRITE_EXT_CMD         0x2A
#define WRITE_EXT_CMDLEN      10
class WriteXCmd : public ScsiCmd {
 public:
  WriteXCmd(unsigned address, unsigned sectors, 
	   unsigned char *inbuf, bool FUA = FALSE) throw (Exception*);
  void SetParam( unsigned int lba, unsigned int sectors, 
                unsigned char *inbuf,  bool FUA = FALSE) throw (Exception*);

  virtual void Print();

  virtual const char* getCmdName() { return "WriteXCmd";}
};


// SeekX command

#define SEEK_EXT_CMD         0x2B
#define SEEK_EXT_CMDLEN      10
class SeekXCmd : public ScsiCmd {
 public:
  SeekXCmd(unsigned address) throw (Exception*);
  void SetParam(unsigned int lba) throw (Exception*);
  virtual const char* getCmdName() { return "SeekXCmd";}
};


// ReadCapacity command

#define READ_CAPACITY_CMD    0x25
#define READ_CAPACITY_CMDLEN      10
class ReadCapacityCmd : public ScsiCmd {
 public:
  ReadCapacityCmd() throw (Exception*);

  // capacity in blocks
  unsigned getCapacity();

  // block size in bytes
  unsigned getBlockSize();

  virtual const char* getCmdName() { return "ReadCapacityCmd";}

  virtual void Print();
};


/////////////////////////////////////////////////////////////////
/// Mode Pages
/////////////////////////////////////////////////////////////////

////////////////////////////////
// This is the base class for Mode Select/Sense Mode pages
// ModePages are intended to be used in the following way:
// 1) A ModePage is instantiated.
// 2) The page is passed to the ScsiCmd which will use it.  The command's
//    constructor must call the Init() method on the mode page and give 
//    it a buffer.
// 3) After the command is instantiated, Set* methods can be called on the
//    page to set it to the proper values.
// 4) If the command instantiates sucessfully (doesn't throw an exception), 
//    the command becomes responsible for deleting the page.
class ModePage {
 public:
  ModePage() : my_buffer(NULL) {}
  virtual ~ModePage() { my_buffer = NULL; }

  // This is called by the ScsiCmd which 
  virtual bool Init(unsigned char* buffer);
  
  virtual unsigned char getPageSize() = 0;
  virtual unsigned char getPageCode() = 0;

  virtual void Print();

  // This is a generic Set() for ModePages.  If both of the pages are of
  // the same type, the target will get set exactly the same as the source
  bool Set(ModePage* source);
  
 protected:
  unsigned char *my_buffer;
};


class CachingModePage : public ModePage {
 public:
  CachingModePage() : ModePage() {}
  
  bool getWriteCache();
  void setWriteCache(bool value);

  bool getReadCache();
  void setReadCache(bool value);

  bool getAbortPrefetch();
  void setAbortPrefetch(bool value);

  unsigned getMinPrefetch();
  void setMinPrefetch(unsigned value);

  unsigned getMaxPrefetch();
  void setMaxPrefetch(unsigned value);

  unsigned char getCacheSegments();
  unsigned getCacheSegmentSize();

  virtual unsigned char getPageSize();
  virtual unsigned char getPageCode();

  virtual void Print();

};

class FormatModePage : public ModePage {
 public:
  FormatModePage() : ModePage() {}
  
  unsigned getTracksPerZone();
  unsigned getSectorsPerTrack();
  unsigned getBytesPerSector();
  unsigned getTrackSkew();
  unsigned getCylinderSkew();

  virtual unsigned char getPageSize();
  virtual unsigned char getPageCode();

  virtual void Print();

};

class RigidGeomModePage : public ModePage {
 public:
  RigidGeomModePage() : ModePage() {}
  
  unsigned getCylinders();
  unsigned char getHeads();
  unsigned getRotationRate();

  virtual unsigned char getPageSize();
  virtual unsigned char getPageCode();
  
  virtual void Print();
  
};


/////////////////////////////////////////////////////////////////
/// ModeSense and ModeSelect ScsiCmds
/////////////////////////////////////////////////////////////////

#define MODE_PAGE_REPLY_HEADER_LEN     4
/*
 * ModeSenseCmd result_data:
 * +-------------------+
 * | Mode Reply Header | <- MODE_SENSE_REPLY_HEADER_LEN
 * +-------------------+
 * | Mode Page Data    | <- Mode Page size
 * +-------------------+
 */

#define MODE_SENSE_CMD         0x1A
#define MODE_SENSE_CMDLEN      6

// Mode Sense Page Control Field values
enum  ModeSensePC {
  MS_CurrentValues = 0x00,
  MS_ChangableValues = 0x01,
  MS_DefaultValues = 0x02,
  MS_SavedValues = 0x03,
};

class ModeSenseCmd : public ScsiCmd {
 public:
  ModeSenseCmd(ModePage* pageMode, ModeSensePC pcf) throw (Exception*);

  virtual void Print();

  ModePage *getPage();

  virtual const char* getCmdName() { return "ModeSenseCmd";};

 protected:
  ModePage *my_page;
};


/*
 * ModeSelectCmd in_data:
 * +-------------------+
 * | Mode Reply Header | <- MODE_SENSE_REPLY_HEADER_LEN
 * +-------------------+
 * | Mode Page Data    | <- Mode Page size
 * +-------------------+
 */

#define MODE_SELECT_CMD         0x15
#define MODE_SELECT_CMDLEN      6
class ModeSelectCmd : public ScsiCmd {
 public:
  ModeSelectCmd(ModePage* pageMode, bool saveValues) throw (Exception*);

  virtual void Print();

  ModePage *getPage();
 
  virtual const char* getCmdName() { return "ModeSelectCmd";};

 protected:
  ModePage *my_page;
};



/////////////////////////////////////////////////////////////////
/// Diagnostic Pages
/////////////////////////////////////////////////////////////////

class DiagPage {
 public:
  DiagPage() : my_buffer(NULL) {}
  virtual ~DiagPage() { my_buffer = NULL; }

  virtual bool Init(unsigned char* buffer) = 0;
  
  virtual unsigned char getPageSize() = 0;
  virtual unsigned char getPageCode() = 0;

  virtual void Print();

  bool isValidLogical(){
    if(!my_buffer) return FALSE;
    return my_buffer[5]==0;
  }
  
 protected:
  unsigned char *my_buffer;
};

////////////////////////////////////////////////////////////////////////////////
// TransAddrDiagPage is used by Send/ReceiveDiag commands to
// translate between logical and physical addresses.
//
// NOTE about sourceFormat:
// This page is used by both the Send and Receive Diagnostic commands.  The 
// page itself contains flags specifying logical to physical, or physical to 
// logical translation.  Unfortunatly, these flags don't specify whether the 
// page is the source form (passed to the Send Diagnostic command), or the 
// translated form (recieved from the Receive Diagnostic command).  
// To deal with this, this class maintains a flag (sourceFormat) which says
// if the page is in source or translated form.  The page gets put into source
// format only if a Set method is made to the page.
//
#define TRANS_ADDR_LOGICAL_FORM      0x00
#define TRANS_ADDR_PHYSICAL_FORM     0x05

#define TRANS_ADDR_DIAG_CODE         0x40
#define TRANS_ADDR_DIAG_PAGELEN      14
class TransAddrDiagPage : public DiagPage {
 public:
  TransAddrDiagPage() : DiagPage(), sourceFormat(FALSE) {}
  
  virtual bool Init(unsigned char* buffer);

  // sets the page as logical->physical, with the supplied address
  // also sets the page into source format
  void setLogical(unsigned int addr);

  unsigned int getLogical();

  // sets the page as physical->logical, with the supplied address
  // also sets the page into source format
  void setPhysical(unsigned cylinder, unsigned char head, unsigned sector);
  unsigned int getCylinder();
  unsigned char getHead();
  unsigned int getSector();

  bool isLogical();
  
  virtual unsigned char getPageSize();
  virtual unsigned char getPageCode();

  virtual void Print();

 protected:
  bool sourceFormat; //is this in the supplied or translated format?
};


/////////////////////////////////////////////////////////////////
/// SendDiag and RecvDiag ScsiCmds
/////////////////////////////////////////////////////////////////

#define RECV_DIAG_CMD         0x1C
#define RECV_DIAG_CMDLEN      6
class ReceiveDiagCmd : public ScsiCmd {
 public:
  ReceiveDiagCmd(DiagPage *page) throw (Exception*);

  DiagPage *getPage();

  virtual void Print();

  virtual const char* getCmdName() { return "RecvDiagCmd";};

 protected:
  DiagPage *my_page;
};

#define SEND_DIAG_CMD         0x1D
#define SEND_DIAG_CMDLEN      6
class SendDiagCmd : public ScsiCmd {
 public:
  SendDiagCmd(DiagPage *page) throw (Exception*);

  DiagPage *getPage();

  virtual void Print();

  virtual const char* getCmdName() { return "SendDiagCmd";};

 protected:
  DiagPage *my_page;
};

#endif //SCSICMD_H
