#ifndef __ARC_PAYLOADFILE_H__
#define __ARC_PAYLOADFILE_H__

#include <vector>

#include <arc/FileAccess.h>
#include <arc/message/PayloadRaw.h>
#include <arc/message/PayloadStream.h>

namespace ARex {

/** Implementation of PayloadRawInterface which provides access to ordinary file.
  Currently only read-only mode is supported. */
class PayloadFile: public Arc::PayloadRawInterface {
 protected:
  /* TODO: use system-independent file access */
  int handle_;
  char* addr_;
  off_t size_; 
  off_t start_; 
  off_t end_; 
  void SetRead(int h,Size_t start,Size_t end);
 public:
  /** Creates object associated with file for reading from it.
    Use end=-1 for full size. */
  PayloadFile(const char* filename,Size_t start,Size_t end);
  PayloadFile(int h,Size_t start,Size_t end);
  /** Creates object associated with file for writing into it.
    Use size=-1 for undefined size. */
  //PayloadFile(const char* filename,Size_t size);
  virtual ~PayloadFile(void);
  virtual char operator[](Size_t pos) const;
  virtual char* Content(Size_t pos = -1);
  virtual Size_t Size(void) const;
  virtual char* Insert(Size_t pos = 0,Size_t size = 0);
  virtual char* Insert(const char* s,Size_t pos = 0,Size_t size = -1);
  virtual char* Buffer(unsigned int num);
  virtual Size_t BufferSize(unsigned int num) const;
  virtual Size_t BufferPos(unsigned int num) const;
  virtual bool Truncate(Size_t size);

  operator bool(void) { return (handle_ != -1); };
  bool operator!(void) { return (handle_ == -1); };
};

class PayloadBigFile: public Arc::PayloadStream {
 private:
  static Size_t threshold_;
  off_t limit_; 
 public:
  /** Creates object associated with file for reading from it */
  PayloadBigFile(const char* filename,Size_t start,Size_t end);
  PayloadBigFile(int h,Size_t start,Size_t end);
  /** Creates object associated with file for writing into it.
    Use size=-1 for undefined size. */
  //PayloadBigFile(const char* filename,Size_t size);
  virtual ~PayloadBigFile(void);
  virtual Size_t Pos(void) const;
  virtual Size_t Size(void) const;
  virtual Size_t Limit(void) const;
  virtual bool Get(char* buf,int& size);

  operator bool(void) { return (handle_ != -1); };
  bool operator!(void) { return (handle_ == -1); };
  static Size_t Threshold(void) { return threshold_; };
  static void Threshold(Size_t t) { if(t > 0) threshold_=t; };
};

class PayloadFAFile: public Arc::PayloadStreamInterface {
 protected:
  Arc::FileAccess* handle_;
  off_t limit_; 
 public:
  /** Creates object associated with file for reading from it */
  PayloadFAFile(Arc::FileAccess* h,Size_t start,Size_t end);
  virtual ~PayloadFAFile(void);
  virtual Size_t Pos(void) const;
  virtual Size_t Size(void) const;
  virtual Size_t Limit(void) const;
  virtual bool Get(char* buf,int& size);
  virtual bool Get(std::string& buf) {
    char cbuf[1024];
    int size = sizeof(cbuf);
    if(!Get(cbuf,size)) return false;
    buf.assign(cbuf,size);
    return true;
  };
  virtual std::string Get(void) { std::string buf; Get(buf); return buf; };
  virtual bool Put(const char* buf,Size_t size) { return false; };
  virtual bool Put(const std::string& buf) { return Put(buf.c_str(),buf.length()); };
  virtual bool Put(const char* buf) { return Put(buf,buf?strlen(buf):0); };
  virtual int Timeout(void) const { return 0; };
  virtual void Timeout(int to) { };
  operator bool(void) { return (handle_ != NULL); };
  bool operator!(void) { return (handle_ == NULL); };
};

// For ranges start is inclusive and end is exclusive
Arc::MessagePayload* newFileRead(const char* filename,Arc::PayloadRawInterface::Size_t start = 0,Arc::PayloadRawInterface::Size_t end = (Arc::PayloadRawInterface::Size_t)(-1));
Arc::MessagePayload* newFileRead(int h,Arc::PayloadRawInterface::Size_t start = 0,Arc::PayloadRawInterface::Size_t end = (Arc::PayloadRawInterface::Size_t)(-1));
Arc::MessagePayload* newFileRead(Arc::FileAccess* h,Arc::PayloadRawInterface::Size_t start = 0,Arc::PayloadRawInterface::Size_t end = (Arc::PayloadRawInterface::Size_t)(-1));

} // namespace ARex

#endif /* __ARC_PAYLOADFILE_H__ */
