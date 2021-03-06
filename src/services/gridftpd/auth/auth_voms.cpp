#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <globus_gsi_credential.h>

#include <vector>

#include <arc/globusutils/GlobusErrorUtils.h>
#include <arc/credential/VOMSUtil.h>
#include <arc/Utils.h>
#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/ArcConfigIni.h>

#include "auth.h"

static Arc::Logger logger(Arc::Logger::getRootLogger(),"AuthUserVOMS");

static AuthResult process_vomsproxy(const char* filename,std::vector<struct voms_t> &data,bool auto_cert = false);

AuthResult AuthUser::process_voms(void) {
  if(!voms_extracted) {
    if(filename.length() > 0) {
      AuthResult err = process_vomsproxy(filename.c_str(),voms_data);
      voms_extracted=true;
      logger.msg(Arc::DEBUG, "VOMS proxy processing returns: %i - %s", err, err_to_string(err));
      if(err != AAA_POSITIVE_MATCH) return err;
    };
  };
  return AAA_POSITIVE_MATCH;
}

AuthResult AuthUser::match_voms(const char* line) {
  // parse line
  std::string vo("");
  std::string group("");
  std::string role("");
  std::string capabilities("");
  std::string auto_c("");
  int n;
  n=Arc::ConfigIni::NextArg(line,vo,' ','"');
  if(n == 0) {
    logger.msg(Arc::ERROR, "Missing VO in configuration");
    return AAA_FAILURE;
  };
  line+=n;
  n=Arc::ConfigIni::NextArg(line,group,' ','"');
  if(n == 0) {
    logger.msg(Arc::ERROR, "Missing group in configuration");
    return AAA_FAILURE;
  };
  line+=n;
  n=Arc::ConfigIni::NextArg(line,role,' ','"');
  if(n == 0) {
    logger.msg(Arc::ERROR, "Missing role in configuration");
    return AAA_FAILURE;
  };
  line+=n;
  n=Arc::ConfigIni::NextArg(line,capabilities,' ','"');
  if(n == 0) {
    logger.msg(Arc::ERROR, "Missing capabilities in configuration");
    return AAA_FAILURE;
  };
  n=Arc::ConfigIni::NextArg(line,auto_c,' ','"');
  logger.msg(Arc::VERBOSE, "Rule: vo: %s", vo);
  logger.msg(Arc::VERBOSE, "Rule: group: %s", group);
  logger.msg(Arc::VERBOSE, "Rule: role: %s", role);
  logger.msg(Arc::VERBOSE, "Rule: capabilities: %s", capabilities);
  // extract info from voms proxy
  // if(voms_data->size() == 0) {
  if(process_voms() != AAA_POSITIVE_MATCH) return AAA_FAILURE;
  if(voms_data.empty()) return AAA_NO_MATCH;
  // analyse permissions
  for(std::vector<struct voms_t>::iterator v = voms_data.begin();v!=voms_data.end();++v) {
    logger.msg(Arc::DEBUG, "Match vo: %s", v->voname);
    if((vo == "*") || (vo == v->voname)) {
      bool matched = false;
      for(std::vector<struct voms_fqan_t>::iterator f = v->fqans.begin(); f != v->fqans.end(); ++f) {
        if(((group == "*") || (group == f->group)) &&
           ((role == "*") || (role == f->role)) &&
           ((capabilities == "*") || (capabilities == f->capability))) {
          if(!matched) {
            default_voms_ = voms_t();
            default_voms_.voname = v->voname;
            default_voms_.server = v->server;
            matched = true;
          };
          default_voms_.fqans.push_back(*f);
        };
      };
      if(matched) {
        return AAA_POSITIVE_MATCH;
      };
    };
  };
  logger.msg(Arc::VERBOSE, "Matched nothing");
  return AAA_NO_MATCH;
}


static AuthResult process_vomsproxy(const char* filename,std::vector<struct voms_t> &data,bool /* auto_cert */) {
  std::string voms_dir = "/etc/grid-security/vomsdir";
  std::string cert_dir = "/etc/grid-security/certificates";
  {
    std::string v;
    if(!(v = Arc::GetEnv("X509_VOMS_DIR")).empty()) voms_dir = v;
    if(!(v = Arc::GetEnv("X509_CERT_DIR")).empty()) cert_dir = v;
  };
  std::string voms_processing = Arc::GetEnv("VOMS_PROCESSING");
  Arc::Credential c(filename, filename, cert_dir, "");
  std::vector<Arc::VOMSACInfo> output;
  std::string emptystring = "";
/*
  Arc::VOMSTrustList emptylist;
  emptylist.AddRegex(".*");
*/  
  std::string voms_trust_chains = Arc::GetEnv("VOMS_TRUST_CHAINS");
  logger.msg(Arc::VERBOSE, "VOMS trust chains: %s", voms_trust_chains);

  std::vector<std::string> vomstrustlist;

  std::vector<std::string> vomstrustchains;
  Arc::tokenize(voms_trust_chains, vomstrustchains, "\n");
  for(size_t i=0; i<vomstrustchains.size(); i++) {
    std::vector<std::string> vomstrust_dns;
    std::string trust_chain = vomstrustchains[i];
    std::string::size_type p1, p2=0;
    while(1) {
      p1 = trust_chain.find("\"", p2);
      if(p1!=std::string::npos) {
        p2 = trust_chain.find("\"", p1+1);
        if(p2!=std::string::npos) {
          std::string str = trust_chain.substr(p1+1, p2-p1-1);
          vomstrust_dns.push_back(str);
          p2++; if(trust_chain[p2] == '\n') break;
        }
      }
      if((p1==std::string::npos) || (p2==std::string::npos)) break;
    }
    if(!vomstrust_dns.empty()) {
      if(vomstrustlist.empty())
        vomstrustlist.insert(vomstrustlist.begin(), vomstrust_dns.begin(), vomstrust_dns.end());
      else {
        vomstrustlist.push_back("----NEXT CHAIN---");
        vomstrustlist.insert(vomstrustlist.end(), vomstrust_dns.begin(), vomstrust_dns.end());
      }
    }
  }

  Arc::VOMSTrustList voms_trust_list(vomstrustlist);
  parseVOMSAC(c, cert_dir, emptystring, voms_dir, voms_trust_list, output, true, true);
  for(size_t n=0;n<output.size();++n) {
    if(!(output[n].status & Arc::VOMSACInfo::Error)) {
      data.push_back(AuthUser::arc_to_voms(output[n].voname,output[n].attributes));
    } else {
      if(voms_processing == "relaxed") {
      } else if(voms_processing == "standard") {
        if(output[n].status & Arc::VOMSACInfo::IsCritical) goto error_exit;
      } else if(voms_processing == "strict") {
        if(output[n].status & Arc::VOMSACInfo::IsCritical) goto error_exit;
        if(output[n].status & Arc::VOMSACInfo::ParsingError) goto error_exit;
      } else if(voms_processing == "noerrors") {
        goto error_exit;
      } else { // == standard
        if(output[n].status & Arc::VOMSACInfo::IsCritical) goto error_exit;
      };
    };
  };
  ERR_clear_error();
  return AAA_POSITIVE_MATCH;
error_exit:
  ERR_clear_error();
  return AAA_FAILURE;
}
