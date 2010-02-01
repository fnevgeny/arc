#include <vector>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#include <arc/StringConv.h>
#include <arc/Logger.h>

#include "SRMInfo.h"

Arc::Logger SRMInfo::logger(Arc::Logger::getRootLogger(), "SRMInfo");

bool SRMFileInfo::operator ==(SRMURL srm_url) {
  const std::string proto_val = srm_url.Option("protocol");
  if (host == srm_url.Host() &&
      (!srm_url.PortDefined() || port == srm_url.Port()) &&
      (proto_val.empty() || ((protocol == "gssapi") == srm_url.GSSAPI())) &&
      version == srm_url.SRMVersion())
    return true;
  return false;
}

std::string SRMFileInfo::versionString() const {
  switch (version) {
    case SRMURL::SRM_URL_VERSION_1: {
      return "1";
    }; break;
    case SRMURL::SRM_URL_VERSION_2_2: {
      return "2.2";
    }; break;
    default: {
      return "";
    }
  }
  return "";
}

Arc::SimpleCondition * SRMInfo::filelock = new Arc::SimpleCondition();

SRMInfo::SRMInfo(std::string dir) {
  srm_info_filename = dir + G_DIR_SEPARATOR_S + "srms.conf";
}

bool SRMInfo::getSRMFileInfo(SRMFileInfo& srm_file_info) {
  
  struct stat fileStat;
  int err = stat( srm_info_filename.c_str(), &fileStat ); 
  if (0 != err || fileStat.st_size == 0) {
    if (0 != err && errno != ENOENT)
      logger.msg(Arc::WARNING, "Error reading srm info file %s:%s", srm_info_filename, strerror(errno));
    return false;
  }

  filelock->lock();
  FILE * pFile;
  char mystring [fileStat.st_size+1];
  pFile = fopen (srm_info_filename.c_str(), "r");
  if (pFile == NULL) {
    logger.msg(Arc::WARNING, "Error reading srm info file %s:%s", srm_info_filename, strerror(errno));
    filelock->unlock();
    return false;
  }

  // read in file
  char * res = fgets (mystring, sizeof(mystring), pFile);
  while (res) {
    std::string line(mystring);
    line = Arc::trim(line);
    if (line.length() > 0 && line[0] == '#') {
      res = fgets (mystring, sizeof(mystring), pFile);
      continue;
    }
    // split line
    std::vector<std::string> fields;
    Arc::tokenize(line, fields);
    if (fields.size() != 4) {
      if (line.length() > 0) 
        logger.msg(Arc::WARNING, "Bad format detected in file %s, in line %s", srm_info_filename, line);
      res = fgets (mystring, sizeof(mystring), pFile);
      continue;
    }
    // look for our combination of host and version
    if (fields.at(0) == srm_file_info.host && fields.at(3) == srm_file_info.versionString()) {
      int port_i = Arc::stringtoi(fields.at(1));
      if (port_i == 0) {
        logger.msg(Arc::WARNING, "Can't convert string %s to int in file %s, line %s", fields.at(1), srm_info_filename, line);
        res = fgets (mystring, sizeof(mystring), pFile);
        continue;
      }        
      srm_file_info.port = port_i;
      srm_file_info.protocol = fields.at(2);
      fclose(pFile);
      filelock->unlock();
      return true;
    }
    res = fgets (mystring, sizeof(mystring), pFile);
  }
  fclose(pFile);
  filelock->unlock();
  return false;
}

void SRMInfo::putSRMFileInfo(const SRMFileInfo& srm_file_info) {
   
  struct stat fileStat;
  int err = stat( srm_info_filename.c_str(), &fileStat ); 
  if (0 != err || fileStat.st_size == 0) {
    if (0 != err && errno != ENOENT) {
      logger.msg(Arc::WARNING, "Error reading srm info file %s:%s", srm_info_filename, strerror(errno));
      return;
    }
    
    // write new file
    filelock->lock();
    
    FILE * pFile;
    pFile = fopen (srm_info_filename.c_str(), "w");
    if (pFile == NULL) {
      logger.msg(Arc::ERROR, "Error opening srm info file for writing %s:%s", srm_info_filename, strerror(errno));
      filelock->unlock();
      return;
    }
    std::string header("# This file was automatically generated by ARC for caching SRM information.\n");
    header += "# Its format is lines with 4 entries separated by spaces:\n";
    header += "# hostname port protocol(gsi/gssapi) version\n#\n";
    header += "# This file can be freely edited, but it is not advisable while there\n";
    header += "# are on-going transfers. Comments begin with #\n#\n";
    header += srm_file_info.host + ' ' + Arc::tostring(srm_file_info.port) + ' ' + srm_file_info.protocol + ' ' + srm_file_info.versionString() + '\n';                       
    fputs ((char*)header.c_str(), pFile);
    fclose (pFile);
    filelock->unlock();
    return;
  }

  // file already exists, so add this entry/replace existing entry
  filelock->lock();
  FILE * pFile;
  char mystring [fileStat.st_size+1];
  pFile = fopen (srm_info_filename.c_str(), "r");
  if (pFile == NULL) {
    logger.msg(Arc::WARNING, "Error opening srm info file %s:%s", srm_info_filename, strerror(errno));
    filelock->unlock();
    return;
  }

  // read in file
  std::vector<std::string> lines;
  
  char * res = fgets (mystring, sizeof(mystring), pFile);
  while (res) {
    std::string line(mystring);
    line = Arc::trim(line);
    if (line.length() > 0 && line[0] == '#') {
      lines.push_back(line+'\n');
      res = fgets (mystring, sizeof(mystring), pFile);
      continue;
    }
    // split line
    std::vector<std::string> fields;
    Arc::tokenize(line, fields);
    if (fields.size() != 4) {
      if (line.length() > 0) 
        logger.msg(Arc::WARNING, "Bad format detected in file %s, in line %s", srm_info_filename, line);
      res = fgets (mystring, sizeof(mystring), pFile);
      continue;
    }
    // if any line contains our combination of host and version, ignore it
    if (fields.at(0) == srm_file_info.host && fields.at(3) == srm_file_info.versionString()) {
      res = fgets (mystring, sizeof(mystring), pFile);
      continue;
    }
    lines.push_back(line+'\n');
    res = fgets (mystring, sizeof(mystring), pFile);
  }
  fclose(pFile);

  // write everything back to the file
  pFile = fopen (srm_info_filename.c_str(), "w");
  if (pFile == NULL) {
    logger.msg(Arc::WARNING, "Error opening srm info file for writing %s:%s", srm_info_filename, strerror(errno));
    filelock->unlock();
    return;
  }
  
  for (std::vector<std::string>::iterator i = lines.begin(); i != lines.end(); i++) {
    fputs ((char*)i->c_str(), pFile);
  }
  std::string this_info = srm_file_info.host + ' ' + Arc::tostring(srm_file_info.port) + ' ' + srm_file_info.protocol + ' ' + srm_file_info.versionString() + '\n';                       
  fputs ((char*)this_info.c_str(), pFile);
  fclose (pFile);
  filelock->unlock();
}
