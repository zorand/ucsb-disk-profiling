#include "basics.h"
#include "scsicmd.h"


///////////////////////////////////
/// InquiryCmd
///////////////////////////////////

/* request vendor brand and model */
InquiryCmd::InquiryCmd()
  throw (Exception*) : ScsiCmd()
{

  my_info = new scsi_cmd_info(INQUIRY_CMDLEN, 0, FALSE, 
			      INQUIRY_REPLY_LEN, FALSE);
  ASSERT(  my_info->result_data != NULL );

  unsigned char* cmdblk = my_info->cmd;
  memset(cmdblk, 0, INQUIRY_CMDLEN);

  cmdblk[0] = INQUIRY_CMD;
  cmdblk[4] = INQUIRY_REPLY_LEN;
}

const char* InquiryCmd::getVendor() {
  if (!isDone()) return "";

  strncpy(vendor_buf, (char*)my_info->result_data + 8, 8);
  
  int i = 8;
  do {
    vendor_buf[i--] = '\0';
  } while (vendor_buf[i] == ' ');

  return vendor_buf;
}

const char* InquiryCmd::getProduct() {
  if (!isDone()) return "";

  strncpy(product_buf, (char*)my_info->result_data + 16, 16);

  int i = 16;
  do {
    product_buf[i--] = '\0';
  } while (product_buf[i] == ' ');

  return product_buf;
}

const char* InquiryCmd::getProductRev() {
  if (!isDone()) return "";

  strncpy(product_rev_buf, (char*)my_info->result_data + 32, 4);

  int i = 4;
  do {
    product_rev_buf[i--] = '\0';
  } while (product_rev_buf[i] == ' ');

  return product_rev_buf;
}

bool InquiryCmd::isDiskDevice() {
  if (!isDone()) return FALSE;

  return (my_info->result_data[0] == 0x00);
}

unsigned char InquiryCmd::ScsiRevision() {
  if (!isDone()) return FALSE;

  return (my_info->result_data[2] & 0x07 );
}


void InquiryCmd::Print() {
  ScsiCmd::__Print();

  unsigned char* cmdblk = my_info->cmd;
  fprintf(stderr, "EVPD %s\tPage code %d\tAlloc Len %d\tFlag %s\tLink %s\n", 
	  TRUEFALSE(cmdblk[1] & 0x1), cmdblk[2], cmdblk[4], 
	  TRUEFALSE(cmdblk[5] & 0x2), TRUEFALSE( cmdblk[5] & 0x1 ));
  
  if ( isOK() ) {
    unsigned char* results = my_info->result_data;

    char* scsi_names[6] = { "None", "1", "2", "3", "T10/1236", "Unknown"}; 
    unsigned char rev = MIN(ScsiRevision(), 5);

    fprintf(stderr, "Vendor: %s; Product: %s Rev: %s; Type: %s; SCSI revision: %s; Response format %d\n", 
	    getVendor(), getProduct(), getProductRev(),
	    ( isDiskDevice() ? "Direct-Access" : "Other"), 
	    scsi_names[rev],
	    results[3] & 0x0F );
    fprintf(stderr, "Adr32 %s; Adr16 %s; Bus32 %s; Bus16 %s; Sync %s; Link %s; Queuing %s \n",
	    TRUEFALSE(results[6] & 0x2), TRUEFALSE(results[6] & 0x1), 
	    TRUEFALSE(results[7] & 0x40), TRUEFALSE( results[7] & 0x20 ),
	    TRUEFALSE(results[7] & 0x10), TRUEFALSE( results[7] & 0x8 ),
	    TRUEFALSE(results[7] & 0x2));
    

  }
  fprintf(stderr, "\n");
}

///////////////////////////////////
///////////////////////////////////


///////////////////////////////////
/// ReadXCmd
///////////////////////////////////

ReadXCmd::ReadXCmd( unsigned int lba, unsigned int sectors, 
		    bool FUA, bool bitbucket ) 
throw (Exception*) : ScsiCmd()
{
  SetParam(lba, sectors, FUA, bitbucket);
}

void ReadXCmd::SetParam( unsigned int lba, unsigned int sectors, 
			 bool FUA, bool bitbucket ) throw (Exception*)
{
  if ( my_info == NULL )
    my_info = new scsi_cmd_info(READ_EXT_CMDLEN, 0, FALSE,
				sectors*BLOCK_SIZE, bitbucket);
  else
    my_info->resize(READ_EXT_CMDLEN, 0, FALSE, sectors*BLOCK_SIZE, bitbucket);
    
  unsigned char* cmdblk = my_info->cmd;
  memset(cmdblk, 0, READ_EXT_CMDLEN);

  cmdblk[0] = READ_EXT_CMD;
  SET_UCHAR_FROM_BIT( cmdblk + 1, 3, FUA);
  SET_UCHAR4_FROM_UINT( cmdblk + 2, lba);
  SET_UCHAR2_FROM_UINT( cmdblk + 7, sectors);
}

void ReadXCmd::Print() {
  ScsiCmd::__Print();

  unsigned lba = UCHAR4_TO_UINT( my_info->cmd + 2 );
  unsigned sectors = UCHAR2_TO_UINT( my_info->cmd + 7 );
  
  fprintf(stderr, "LBA %Xh; Sectors %u\n\n", lba, sectors);
}

///////////////////////////////////
///////////////////////////////////


///////////////////////////////////
/// WriteXCmd
///////////////////////////////////
WriteXCmd::WriteXCmd(unsigned int lba, unsigned int sectors,
                     unsigned char *inbuf, bool FUA)
throw (Exception*) : ScsiCmd()
{
  SetParam(lba, sectors, inbuf, FUA);
}

void WriteXCmd::SetParam( unsigned int lba, unsigned int sectors, 
                unsigned char *inbuf,  bool FUA ) throw (Exception*)
{
  // if inbuf == NULL, we can do a bitbucket write
  bool bit_bucket = (inbuf == NULL);

  if ( my_info == NULL) 
    my_info = new scsi_cmd_info(WRITE_EXT_CMDLEN, 
				sectors*BLOCK_SIZE, bit_bucket,
				0, FALSE);
  else
    my_info->resize(WRITE_EXT_CMDLEN, sectors*BLOCK_SIZE, bit_bucket,0, FALSE);

  unsigned char* cmdblk = my_info->cmd;
  memset(cmdblk, 0, WRITE_EXT_CMDLEN);

  cmdblk[0] = WRITE_EXT_CMD;
  SET_UCHAR_FROM_BIT( cmdblk + 1, 3, FUA);
  SET_UCHAR4_FROM_UINT( cmdblk + 2, lba);
  SET_UCHAR2_FROM_UINT( cmdblk + 7, sectors);

  // if inbuf was supplied, copy it
  if ( inbuf != NULL && my_info->in_data )
    memcpy( my_info->in_data, inbuf, sectors*BLOCK_SIZE);
}

void WriteXCmd::Print() {
  ScsiCmd::__Print();

  unsigned lba = UCHAR4_TO_UINT( my_info->cmd + 2 );
  unsigned sectors = UCHAR2_TO_UINT( my_info->cmd + 7 );
  
  fprintf(stderr, "LBA %Xh; Sectors %u\n\n", lba, sectors);
}


///////////////////////////////////
///////////////////////////////////

///////////////////////////////////
/// SeekXCmd
///////////////////////////////////
SeekXCmd::SeekXCmd(unsigned int lba)
  throw (Exception*) : ScsiCmd()
{
  
//  my_info = new scsi_cmd_info(SEEK_EXT_CMDLEN, 0, FALSE, 0, FALSE);
//  unsigned char cmdblk [ SEEK_EXT_CMDLEN ] = 
//  {                   SEEK_EXT_CMD,  /* command */
//		      0, /* LUN */
//		      (unsigned char)(lba>>24),  /* MSB */
//		      (unsigned char)(lba>>16),  /*  */
//		      (unsigned char)(lba>>8),  /* logical block */ 
//		      (unsigned char) lba,  /* LSB */
//		      0,
//		      0,
//		      0,
//		      0 };/* reserved/flag/link */
//
//  memcpy( my_info->cmd, cmdblk, sizeof(cmdblk) );

  SetParam(lba);
  
}

void SeekXCmd::SetParam( unsigned int lba) throw (Exception*)
{
  if ( my_info == NULL )
    my_info = new scsi_cmd_info(SEEK_EXT_CMDLEN, 0, FALSE, 0, FALSE);
  else
    my_info->resize(SEEK_EXT_CMDLEN, 0, FALSE, 0, FALSE);
    
  unsigned char cmdblk [ SEEK_EXT_CMDLEN ] = 
  {                   SEEK_EXT_CMD,  /* command */
		      0, /* LUN */
		      (unsigned char)(lba>>24),  /* MSB */
		      (unsigned char)(lba>>16),  /*  */
		      (unsigned char)(lba>>8),  /* logical block */ 
		      (unsigned char) lba,  /* LSB */
		      0,
		      0,
		      0,
		      0 };/* reserved/flag/link */
  
  memcpy( my_info->cmd, cmdblk, sizeof(cmdblk) );

}


///////////////////////////////////
///////////////////////////////////


///////////////////////////////////
/// ReadCapacityCmd
///////////////////////////////////
ReadCapacityCmd::ReadCapacityCmd()
  throw (Exception*) : ScsiCmd()
{
  
  my_info = new scsi_cmd_info(READ_CAPACITY_CMDLEN, 0, FALSE, 8, FALSE);

  unsigned char cmdblk [ READ_CAPACITY_CMDLEN ] = 
  {                   READ_CAPACITY_CMD,  /* command */
		      0, /* LUN, reserved, RelAdr */
		      0, 0, 0, 0, /* LBA */
		      0, 0, 0, /* reserved */
		      0 };/* reserved/flag/link */
  
  memcpy( my_info->cmd, cmdblk, sizeof(cmdblk) );
}

unsigned ReadCapacityCmd::getCapacity() {
  if (!isOK()) return 0;
  return  UCHAR4_TO_UINT(my_info->result_data);
}

unsigned ReadCapacityCmd::getBlockSize() {
  if (!isOK()) return 0;
  return  UCHAR4_TO_UINT(my_info->result_data + 4);
}

void ReadCapacityCmd::Print() {
  ScsiCmd::__Print();

  if (isOK())
    fprintf(stderr, "Capacity %u; Block size %u\n",
	    getCapacity(), getBlockSize());

  fprintf(stderr, "\n");
}

///////////////////////////////////
///////////////////////////////////

///////////////////////////////////
/// ModeSenseCmd
///////////////////////////////////
ModeSenseCmd::ModeSenseCmd( ModePage* modePage, ModeSensePC pcf)
  throw (Exception*) : ScsiCmd(), my_page(NULL)
{
  // check range on PCF
  if ( pcf < MS_CurrentValues || pcf > MS_SavedValues )
    throw new Exception("ModeSenseCmd: invalid pcf value (%u)\n", pcf);

  unsigned char pageCode = modePage->getPageCode();
  unsigned char pageSize = modePage->getPageSize();
  unsigned int replySize = MODE_PAGE_REPLY_HEADER_LEN + pageSize;

  // check range on pageSize
  if ( pageSize <= 0 )
    throw new Exception("ModeSenseCmd: invalid mode page size (%u)\n", pageSize);
  
  my_info = new scsi_cmd_info(MODE_SENSE_CMDLEN, 0, FALSE, replySize, FALSE);

  my_page = modePage;
  
  unsigned char cmdblk [ MODE_SENSE_CMDLEN ] = {
    MODE_SENSE_CMD,  /* command */
    (TRUE << 3), /* LUN = 0, DBD = TRUE */
    (unsigned char)((pcf << 6 & 0xC0) | (pageCode & 0x3F)), /* PCF, pageCode */
    0,
    (unsigned char) replySize, /* alloc length*/
    0 };/* reserved/flag/link */
  
  memcpy( my_info->cmd, cmdblk, sizeof(cmdblk) );

  my_page->Init( my_info->result_data + MODE_PAGE_REPLY_HEADER_LEN );
}

void mode_page_reply_header_print(unsigned char* buffer) {
  if (buffer == NULL) return;

  fprintf(stderr, "Mode Parameter Header:\n");

  fprintf(stderr, "Data len %u\tMedium type %u\tBlock desc len %u\n", 
	  (unsigned char)buffer[0], 
	  (unsigned char)buffer[1], 
	  (unsigned char)buffer[3]
	  );
}

void ModeSenseCmd::Print() {
  ScsiCmd::__Print();

  if (isOK()) {
    mode_page_reply_header_print(my_info->result_data);

    if ( my_page )
      my_page->Print();
  }

  fprintf(stderr, "\n");
}

ModePage* ModeSenseCmd::getPage() {
  return my_page;
}
///////////////////////////////////
///////////////////////////////////


///////////////////////////////////
/// ModeSelectCmd
///////////////////////////////////
ModeSelectCmd::ModeSelectCmd(ModePage* modePage, bool saveValues)
  throw (Exception*) : ScsiCmd(), my_page(NULL)
{
  unsigned char pageSize = modePage->getPageSize();
  unsigned int inSize = MODE_PAGE_REPLY_HEADER_LEN + pageSize;

  // check range on pageSize
  ASSERT( pageSize > 0 );

  
  my_info = new scsi_cmd_info(MODE_SELECT_CMDLEN, inSize, FALSE, 0, FALSE);


  my_page = modePage;
  
  unsigned char cmdblk [ MODE_SELECT_CMDLEN ] = {
    MODE_SELECT_CMD,  /* command */
    (TRUE << 4) | (saveValues & 0x1), /* LUN = 0, PF = TRUE, SMP */
    0,0, /* reserved */ 
    (unsigned char)(inSize),      /* length */
    0 };/* reserved/flag/link */
  
  memcpy( my_info->cmd, cmdblk, sizeof(cmdblk) );

  /* setup the mode select headers */
  unsigned char pageblk [ MODE_PAGE_REPLY_HEADER_LEN] =
  {
    (unsigned char) (pageSize+3), /* mode data len */
    0, /* medium type */
    0, /* reserved */ 
    0 /* block descriptor len */ 
  };

  memcpy( my_info->in_data, pageblk, sizeof(pageblk) );

  my_page->Init( my_info->in_data + MODE_PAGE_REPLY_HEADER_LEN );

}

void ModeSelectCmd::Print() {
  ScsiCmd::__Print();

  mode_page_reply_header_print(my_info->in_data);

  if ( my_page )
    my_page->Print();
}

ModePage* ModeSelectCmd::getPage() {
  return my_page;
}
///////////////////////////////////
///////////////////////////////////

///////////////////////////////////
/// ModePage
///////////////////////////////////

bool ModePage::Init(unsigned char* buffer) {
  my_buffer = buffer;

  /* setup the page */
  unsigned char pageblk [ 2 ] =
  {
    (unsigned char)(getPageCode() & 0x3F ) , /* pageCode */
    (unsigned char)(getPageSize() - 2)  /* pageSize */
  };

  memcpy( my_buffer, pageblk, sizeof(pageblk) );

  return TRUE;
}

void ModePage::Print() {
  fprintf(stderr, "ModePage 0x%2.2x\n", getPageCode());

  if (my_buffer)
    bitprint(my_buffer, getPageSize() );
}

bool ModePage::Set(ModePage *source) {

  // ensure that:
  // 1) the source is not NULL
  // 2) both pages have been Init()'d
  // 3) both pages are the same type
  if ( source == NULL ||
       source->my_buffer == NULL || my_buffer == NULL ||
       getPageCode() != source->getPageCode())
    return FALSE;

  memcpy(my_buffer, source->my_buffer, 
	MIN(getPageSize(),source->getPageSize()) );

  return TRUE;
}


///////////////////////////////////
///////////////////////////////////

///////////////////////////////////
/// CachingModePage
///////////////////////////////////


bool CachingModePage::getWriteCache() {
  if (my_buffer == NULL) return FALSE;

  if (my_buffer[2] & 0x04)
    return TRUE;
  else
    return FALSE;
}

void CachingModePage::setWriteCache(bool value) {
  if (my_buffer == NULL) return;

  SET_UCHAR_FROM_BIT(my_buffer + 2, 2, value);

  // my_buffer[2] = (my_buffer[2] & ~0x4) | (value ? 0x4 : 0);
}

bool CachingModePage::getReadCache() {
  if (my_buffer == NULL) return FALSE;

  if (my_buffer[2] & 0x01)
    return FALSE;
  else
    return TRUE;
}

void CachingModePage::setReadCache(bool value) {
  if (my_buffer == NULL) return;

  SET_UCHAR_FROM_BIT(my_buffer + 2, 0, !value);

  //  my_buffer[2] = (my_buffer[2] & ~0x1) | (value ? 0 : 0x1);
}

void CachingModePage::setAbortPrefetch(bool value) {
  if (my_buffer == NULL) return;

  SET_UCHAR_FROM_BIT(my_buffer + 2, 4, value);
  SET_UCHAR_FROM_BIT(my_buffer + 2, 6, value);

  SET_UCHAR_FROM_BIT(my_buffer + 12, 5, value);
}

bool CachingModePage::getAbortPrefetch() {
  if (my_buffer == NULL) return FALSE;

  return (my_buffer[2] & 0x10 ? TRUE : FALSE);
}

unsigned CachingModePage::getMinPrefetch() {
  if (my_buffer == NULL) return 0;
  return UCHAR2_TO_UINT(my_buffer + 6);
}

void CachingModePage::setMinPrefetch(unsigned value) {
  if (my_buffer == NULL) return;
  SET_UCHAR2_FROM_UINT(my_buffer + 6, value);
}

unsigned CachingModePage::getMaxPrefetch() {
  if (my_buffer == NULL) return 0;
  return UCHAR2_TO_UINT(my_buffer + 8);
}

void CachingModePage::setMaxPrefetch(unsigned value) {
  if (my_buffer == NULL) return;

  SET_UCHAR2_FROM_UINT(my_buffer + 8, value);
}

unsigned CachingModePage::getCacheSegmentSize() {
  if (my_buffer == NULL) return 0;
  return UCHAR2_TO_UINT(my_buffer + 14);
}

unsigned char CachingModePage::getCacheSegments() {
  if (my_buffer == NULL) return 0;
  return my_buffer[13];
}


unsigned char CachingModePage::getPageSize() {
  return 20;
}

unsigned char CachingModePage::getPageCode() {
  return 0x08;
}

void CachingModePage::Print() {
  fprintf(stderr, "ModePage 0x%2.2x (Caching):\n", getPageCode());

  if (my_buffer) {
    fprintf(stderr, "PS %s; Page code 0x%x\nIC %s; ABPF %s; CAP %s; DISC %s; SIZE %s; WCE %s; MF %s; RCD %s\n",
	    TRUEFALSE(my_buffer[0] & 0x80),
	    (unsigned char)(my_buffer[0] & 0x3F),
	    TRUEFALSE(my_buffer[2] & 0x80),
	    TRUEFALSE(my_buffer[2] & 0x40),
	    TRUEFALSE(my_buffer[2] & 0x20),
	    TRUEFALSE(my_buffer[2] & 0x10),
	    TRUEFALSE(my_buffer[2] & 0x08),
	    TRUEFALSE(my_buffer[2] & 0x04),
	    TRUEFALSE(my_buffer[2] & 0x02),
	    TRUEFALSE(my_buffer[2] & 0x01)
	    );
    fprintf(stderr, "Disable Prefetch Length %u; Min Prefetch %u; Max Prefetch %u; Cache Segments %u; Cache Segment Size %u\n",
	    UCHAR2_TO_UINT(my_buffer+4),
	    UCHAR2_TO_UINT(my_buffer+6),
	    UCHAR2_TO_UINT(my_buffer+8),
	    (unsigned char)my_buffer[13],
	    UCHAR2_TO_UINT(my_buffer+14)
	    );

    bitprint(my_buffer, getPageSize());
  }
    fprintf(stderr, "\n");
}

///////////////////////////////////
///////////////////////////////////

///////////////////////////////////
/// FormatModePage
///////////////////////////////////

unsigned FormatModePage::getTracksPerZone() {
  if (my_buffer == NULL) return 0;

  return  UCHAR2_TO_UINT( my_buffer + 2 );
}

unsigned FormatModePage::getSectorsPerTrack() {
  if (my_buffer == NULL) return 0;

  return  UCHAR2_TO_UINT( my_buffer + 10 );
}

unsigned FormatModePage::getBytesPerSector() {
  if (my_buffer == NULL) return 0;

  return  UCHAR2_TO_UINT( my_buffer + 12 );

}

unsigned FormatModePage::getTrackSkew() {
  if (my_buffer == NULL) return 0;

  return  UCHAR2_TO_UINT( my_buffer + 16 );
}

unsigned FormatModePage::getCylinderSkew() {
  if (my_buffer == NULL) return 0;

  return  UCHAR2_TO_UINT( my_buffer + 18 );
}

unsigned char FormatModePage::getPageSize() {
  return 24;
}

unsigned char FormatModePage::getPageCode() {
  return 0x03;
}

void FormatModePage::Print() {
  fprintf(stderr, "ModePage 0x%2.2x (Format Params):\n", getPageCode());

  if (my_buffer) {
    fprintf(stderr, "Tracks/Cyl %u; Sectors/Track %u; Bytes/Sector %u; Track Skew %u; Cylinder Skew %u\n",
	    getTracksPerZone(), getSectorsPerTrack(), getBytesPerSector(),
	    getTrackSkew(), getCylinderSkew());
  }
  fprintf(stderr, "\n");
}

///////////////////////////////////
/// RigidGeomModePage
///////////////////////////////////

unsigned int RigidGeomModePage::getCylinders() {
  if (my_buffer == NULL) return 0;

  return  UCHAR3_TO_UINT( my_buffer + 2 );
}

unsigned char RigidGeomModePage::getHeads() {
  if (my_buffer == NULL) return 0;

  return my_buffer[5];
}

unsigned RigidGeomModePage::getRotationRate() {
  if (my_buffer == NULL) return 0;

  return UCHAR2_TO_UINT( my_buffer + 20 );
}

void RigidGeomModePage::Print() {
  fprintf(stderr, "ModePage 0x%2.2x (Rigid Geometry):\n", getPageCode());

  if (my_buffer) {
    fprintf(stderr, "Cylinders %u; Heads %u; Rotation Rate %u RPM\n",
	    getCylinders(), getHeads(), getRotationRate());
  }
  fprintf(stderr, "\n");
}

unsigned char RigidGeomModePage::getPageSize() {
  return 24;
}

unsigned char RigidGeomModePage::getPageCode() {
  return 0x04;
}

///////////////////////////////////
///////////////////////////////////

///////////////////////////////////
/// DiagPage
///////////////////////////////////

void DiagPage::Print() {
  fprintf(stderr, "DiagPage 0x%2.2x\n", getPageCode());

  if (my_buffer)
    bitprint(my_buffer, getPageSize() );
}

///////////////////////////////////
///////////////////////////////////

///////////////////////////////////
/// TransAddrDiagPage
///////////////////////////////////

bool TransAddrDiagPage::Init(unsigned char* buffer) {
  my_buffer = buffer;

  /* setup the page */
  unsigned char pageblk [ 6 ] =
  {
    (unsigned char)(TRANS_ADDR_DIAG_CODE ) , /* pageCode */
    0, /* reserved */
    (unsigned char)((TRANS_ADDR_DIAG_PAGELEN - 4) >> 8),  /* pageSize, MSB */
    (unsigned char)(TRANS_ADDR_DIAG_PAGELEN - 4),  /* pageSize, LSB */
    0x00, /* sourceFormat (default=logical) */
    0x05 /* translatedFormat (default=physical) */
  };

  // set the page as translated format, it will be set to source
  // format if we call a Set* method on it.
  sourceFormat = FALSE;

  memcpy( my_buffer, pageblk, sizeof(pageblk) );

  return TRUE;
}

void TransAddrDiagPage::setLogical(unsigned address) 
{
  if ( my_buffer == NULL) return;

  // logical to physical
  my_buffer[4] = (unsigned char) TRANS_ADDR_LOGICAL_FORM; 
  my_buffer[5] = (unsigned char) TRANS_ADDR_PHYSICAL_FORM; 

  SET_UCHAR4_FROM_UINT(my_buffer + 6, address);
  SET_UCHAR4_FROM_UINT(my_buffer + 10, 0);

  sourceFormat = TRUE;
}

void TransAddrDiagPage::setPhysical(unsigned cylinder, 
				    unsigned char head, unsigned sector)
{
  if ( my_buffer == NULL) return;

  // physical to logical
  my_buffer[4] = (unsigned char) TRANS_ADDR_PHYSICAL_FORM; 
  my_buffer[5] = (unsigned char) TRANS_ADDR_LOGICAL_FORM; 

  // cylinder
  SET_UCHAR3_FROM_UINT(my_buffer + 6, cylinder);

  // head
  my_buffer[9] = head;

  // sector
  SET_UCHAR4_FROM_UINT(my_buffer + 10, sector);

  sourceFormat = TRUE;
}


unsigned TransAddrDiagPage::getLogical() {
  if ( my_buffer == NULL) return 0;

  return UCHAR4_TO_UINT( my_buffer + 6 );
}

unsigned TransAddrDiagPage::getCylinder() {
  if ( my_buffer == NULL) return 0;
  
  return UCHAR3_TO_UINT( my_buffer + 6 );
}

unsigned char TransAddrDiagPage::getHead() {
  if ( my_buffer == NULL) return 0;

  return my_buffer[9];
}

unsigned TransAddrDiagPage::getSector() {
  if ( my_buffer == NULL) return 0;
  
  return UCHAR4_TO_UINT( my_buffer + 10 );
}

unsigned char TransAddrDiagPage::getPageSize() {
  return TRANS_ADDR_DIAG_PAGELEN;
}

unsigned char TransAddrDiagPage::getPageCode() {
  return TRANS_ADDR_DIAG_CODE;
}


#define PHYSIC_LOGIC(x) ((x == TRANS_ADDR_LOGICAL_FORM) ? "Logical" : \
                         (x == TRANS_ADDR_PHYSICAL_FORM) ? "Physical" : \
                         "Invalid" )

void TransAddrDiagPage::Print() {
  fprintf(stderr, "DiagPage 0x%2.2x (Translate Address):\n", getPageCode());

  if (my_buffer) {
    unsigned char source = my_buffer[4] & 0x7;
    unsigned char target = my_buffer[5] & 0x7;

    fprintf(stderr, "(%s -> %s); ",
	    PHYSIC_LOGIC(source), PHYSIC_LOGIC(target));

    if ( isLogical() )
      fprintf(stderr, "Logical %Xh\n",getLogical());
    else
      fprintf(stderr, "Physical (%Xh, %Xh, %Xh)\n",
	      getCylinder(), getHead(), getSector());

#ifdef SCSI_DEBUG
    bitprint(my_buffer, getPageSize() );
#endif
  }
}

bool TransAddrDiagPage::isLogical() {
  if ( my_buffer == NULL )
    return TRUE;

  bool sourceIsLogical = (my_buffer[4] & 0x7) == TRANS_ADDR_LOGICAL_FORM;
  bool targetIsLogical = (my_buffer[5] & 0x7) == TRANS_ADDR_LOGICAL_FORM;

//if(my_buffer[5])  
//  bitprint(&my_buffer[4],2);
  
  if ( sourceFormat )
    return sourceIsLogical;
  else
    return targetIsLogical;
}

///////////////////////////////////
///////////////////////////////////


///////////////////////////////////
/// SendDiagCmd
///////////////////////////////////
SendDiagCmd::SendDiagCmd(DiagPage* page)
  throw (Exception*) : ScsiCmd(), my_page(NULL)
{
  unsigned char pageSize = page->getPageSize();

  my_info = new scsi_cmd_info(SEND_DIAG_CMDLEN, pageSize, FALSE, 0, FALSE);

  my_page = page;
  
  unsigned char cmdblk [ SEND_DIAG_CMDLEN ] = {
    SEND_DIAG_CMD,  /* command */
    (TRUE << 4), /* LUN = 0, PF = TRUE, others */
    0, /* reserved */ 
    (unsigned char)(pageSize >> 8),      /* length, MSB */
    (unsigned char)(pageSize),           /* length, LSB */
    0 };/* reserved/flag/link */
  
  memcpy( my_info->cmd, cmdblk, sizeof(cmdblk) );

  my_page->Init( my_info->in_data);

}

DiagPage* SendDiagCmd::getPage() {
  return my_page;
}

void SendDiagCmd::Print() {
  ScsiCmd::__Print();

  if (my_page)
    my_page->Print();

  fprintf(stderr, "\n");
}

///////////////////////////////////
///////////////////////////////////

///////////////////////////////////
/// ReceiveDiagCmd
///////////////////////////////////
ReceiveDiagCmd::ReceiveDiagCmd(DiagPage *page)
  throw (Exception*) : ScsiCmd(), my_page(NULL)
{
  unsigned char pageSize = page->getPageSize();

  my_info = new scsi_cmd_info(RECV_DIAG_CMDLEN, 0, FALSE, pageSize, FALSE);

  my_page = page;
  
  unsigned char cmdblk [ RECV_DIAG_CMDLEN ] = {
    RECV_DIAG_CMD,  /* command */
    0 , /* LUN, reserved */
    0, /* reserved */ 
    (unsigned char)(pageSize >> 8),      /* length, MSB */
    (unsigned char)(pageSize),           /* length, LSB */
    0 };/* reserved/flag/link */
  
  memcpy( my_info->cmd, cmdblk, sizeof(cmdblk) );

  my_page->Init( my_info->result_data);
}

void ReceiveDiagCmd::Print() {
  ScsiCmd::__Print();
  
  if (isOK() && my_page)
    my_page->Print();

  fprintf(stderr, "\n");
}


///////////////////////////////////
///////////////////////////////////


///////////////////////////////////
/// struct scsi_cmd_info
///////////////////////////////////

scsi_cmd_info::~scsi_cmd_info(){
  if ( in_buffer )
    delete[] in_buffer;
  if ( result_buffer )
  delete[] result_buffer;

  in_buffer = NULL;
  result_buffer = NULL;

  in_header = NULL;
  cmd = NULL;
  in_data = NULL;

  result_header = NULL;
  result_data = NULL;

  cmd_len = 0;

  in_size = 0;
  result_size = 0;

  in_buffer_size = 0;
  result_buffer_size = 0;

  done = FALSE;
}

scsi_cmd_info::scsi_cmd_info(unsigned cmd_len, unsigned in_len, bool in_bucket,
			     unsigned result_len, bool result_bucket) 
{
  in_buffer_size = SCSI_OFF + cmd_len;
  if ( in_bucket == FALSE ) in_buffer_size += in_len;

  result_buffer_size = SCSI_OFF;
  if ( result_bucket == FALSE ) result_buffer_size += result_len;

  in_buffer = new unsigned char[in_buffer_size];
  result_buffer = new unsigned char[result_buffer_size];

  // setup all the pointers and sizes correctly
  set(cmd_len, in_len, in_bucket, result_len, result_bucket, 
      in_buffer_size, result_buffer_size);
}

void scsi_cmd_info::resize(unsigned new_cmd_len, 
			   unsigned new_in_size, bool new_in_bucket,
			   unsigned new_result_size, bool new_result_bucket) 
{
  // The strategy here is to allow the buffer usage to resize without 
  // allocating new buffers.  We grow buffers if more space is needed.

  // figure out the size of the new buffers
  unsigned new_in_buffer_size = SCSI_OFF + new_cmd_len;
  if ( new_in_bucket == FALSE ) new_in_buffer_size += new_in_size;

  unsigned new_result_buffer_size = SCSI_OFF;
  if ( new_result_bucket == FALSE ) new_result_buffer_size += new_result_size;

  // if we need to grow, reallocate
  if ( new_in_buffer_size > in_buffer_size ) {
    delete[] in_buffer;
    in_buffer = new unsigned char[new_in_buffer_size];
    in_buffer_size = new_in_buffer_size;
  }

  if ( new_result_buffer_size > result_buffer_size ) {
    delete[] result_buffer;
    result_buffer = new unsigned char[new_result_buffer_size];
    result_buffer_size = new_result_buffer_size;
  }

  // setup all the pointers and sizes correctly
  set(new_cmd_len, 
      new_in_size, new_in_bucket, 
      new_result_size, new_result_bucket,
      new_in_buffer_size, new_result_buffer_size);
}

void scsi_cmd_info::set(unsigned cmd_len, unsigned in_len, bool in_bucket,
			unsigned result_len, bool result_bucket,
			unsigned num_in_bytes, unsigned num_result_bytes) 
{
  in_bucket = in_bucket || in_len == 0;
  result_bucket = result_bucket || result_len == 0;

  in_header = (ScsiGenericHeader*)in_buffer;
  cmd = in_buffer + SCSI_OFF;
  if ( !in_bucket )
    in_data = in_buffer + SCSI_OFF + cmd_len;
  else
    in_data = NULL;

  result_header = (ScsiGenericHeader*)result_buffer;
  if ( !result_bucket )
    result_data = result_buffer + SCSI_OFF;
  else
    result_data = NULL;

  this->cmd_len = cmd_len;
  in_size = in_len;
  result_size = result_len;

  start_time = 0;
  stop_time = 0;
  done = FALSE;

  memset((void*)in_buffer, 0, num_in_bytes );
  memset((void*)result_buffer, 0, num_result_bytes );

  //  printf("in: buf %x header %x cmd %x data %x\n",
  //	 in_buffer, in_header, cmd, in_data);
  //  printf("result: buf %x header %x data %x\n",
  //	 result_buffer, result_header, result_data);
}


///////////////////////////////////
///////////////////////////////////


///////////////////////////////////
/// ScsiCmd
///////////////////////////////////

ScsiCmd::~ScsiCmd() {
  if ( my_info )
    delete my_info;
}

scsi_cmd_info* ScsiCmd::getInfo() {
  return my_info;
}

void ScsiCmd::Print() {
  __Print();
  fprintf(stderr, "\n");
}

void ScsiCmd::__Print() {

  if (my_info == NULL)
    return;

  fprintf(stderr, "%s (%Xh):\n", getCmdName(), my_info->cmd[0]); 

  fprintf(stderr, "CmdLen %d; InLen %d; ResultLen %d; Status %s", 
	  my_info->cmd_len, my_info->in_size, my_info->result_size, 
	  ( isDone() == FALSE ? "NotDone" : (isOK() ? "Done" : "FAILED"))
	  );
  
  if ( isDone() ) {
    fprintf(stderr, "; Time %f us\n", 	  
	    ftime(my_info->stop_time,my_info->start_time));

    if ( isOK() == FALSE )
      my_info->result_header->Print();
  } else
    fprintf(stderr, "\n");
  
#ifdef SCSI_CMD_DEBUG
  bitprint(my_info->cmd, my_info->cmd_len);
#endif
  
}

bool ScsiCmd::isOK() {
  if (my_info == NULL)
    return FALSE;
  else
    return (my_info->done && 
	    my_info->result_header->isOK() &&
	    my_info->in_header->isOK());
}

bool ScsiCmd::isDone() {
  if (my_info == NULL)
    return FALSE;
  else
    return my_info->done;
}

///////////////////////////////////
///////////////////////////////////


///////////////////////////////////
///////////////////////////////////
/// ScsiGenericHeader
///////////////////////////////////

void ScsiGenericHeader::Set(int replyLen, bool twelveByte, int packId) {
  // zero out everything
  memset((void*)this ,0 ,SCSI_OFF);

  // set the things we care about
  reply_len = replyLen;
  twelve_byte = twelveByte;
  pack_id = packId;
}

void ScsiGenericHeader::Clear() {
  // zero out everything
  memset((void*)this ,0 ,SCSI_OFF);
}

bool ScsiGenericHeader::isOK() {
  return (target_status == TargetStatus_Good &&
	  host_status == HostStatus_OK &&
	  (driver_status & 0x0F) == DriverStatus_OK );
}

TargetStatus ScsiGenericHeader::targetStatus() {
  return (TargetStatus)target_status;
}

HostStatus ScsiGenericHeader::hostStatus() {
  return (HostStatus)host_status;
}

DriverStatus ScsiGenericHeader::driverStatus() {
  return (DriverStatus)(driver_status & 0x0F);
}

DriverSuggest ScsiGenericHeader::driverSuggestion() {
  return (DriverSuggest)(driver_status & 0xF0);
}

SenseData* ScsiGenericHeader::getSenseData() {
  if ( target_status != TargetStatus_CheckCondition &&
       target_status != TargetStatus_CommandTerminated &&
       (driver_status & DriverStatus_Sense) == FALSE )
    return NULL;

  return (SenseData*)sense_buffer;
}


void ScsiGenericHeader::Print() {

  fprintf(stderr, "SG Header: packet len %d; packet id %d; target status %Xh; host status %Xh; driver_status %Xh; driver suggestion %Xh\n", 
	    pack_len, pack_id, target_status, host_status,
	    driver_status & 0x0F, driver_status & 0xF0);

  SenseData* sense_data = getSenseData();
  if ( sense_data ) {
    sense_data->Print();
  }
}

///////////////////////////////////
///////////////////////////////////

///////////////////////////////////
///////////////////////////////////
/// SenseData
///////////////////////////////////

void SenseData::Print() {
  char* error_code_names[] = {"Current", "Deferred", "Invalid"}; 
  
  fprintf(stderr, "Sense data:\n");
  fprintf(stderr, "valid %s; code %s; ILI %s; Sense key %d; LBA %Xh; Additional Code %2.2Xh %2.2Xh\n", 
	  TRUEFALSE(valid()),
	  error_code_names[errorCode()],
	  TRUEFALSE(ILI()), (senseKey()),
	  informationLBA(),
	  additionalSenseCode(), additionalSenseCodeQualifier());
  
#ifdef DEBUG_SCSI
  bitprint(data, SG_MAX_SENSE);
#endif
}

bool SenseData::valid() {
  return (data[0] & 0x80 ? TRUE : FALSE); 
}

SenseErrorCode SenseData::errorCode() {
  switch (data[0] & 0x7F) {
  case(0x70): return SenseErrorCode_Current;
  case(0x71): return SenseErrorCode_Deferred;
  default: return SenseErrorCode_Invalid;
  }
}

bool SenseData::ILI() {
  return (data[2] & 0x20 ? TRUE : FALSE);
} 

SenseKeys SenseData::senseKey() {
  return (SenseKeys)(data[2] & 0x0F);
}

unsigned SenseData::informationLBA() {
  return UCHAR4_TO_UINT(data+3);
}

unsigned char SenseData::additionalSenseCode() {
  return data[12];
}

unsigned char SenseData::additionalSenseCodeQualifier() {
  return data[13];
}

///////////////////////////////////
///////////////////////////////////
