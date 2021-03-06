#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>
#include <iostream>

#include <arc/loader/Loader.h>
#include <arc/message/PayloadRaw.h>
#include <arc/ArcConfig.h>
#include <arc/StringConv.h>
#include <arc/URL.h>
#include <arc/security/ClassLoader.h>

#include "PolicyParser.h"

using namespace Arc;
using namespace ArcSec;

static Arc::Logger logger(Arc::Logger::rootLogger, "PolicyParser");

PolicyParser::PolicyParser(){
}

/*
///Get policy from local file
void getfromFile(const char* name, std::string& xml_policy){
  std::string str;
  std::ifstream f(name);
  if(!f) logger.msg(ERROR,"Failed to read policy file %s",name);

  while (f >> str) {
    xml_policy.append(str);
    xml_policy.append(" ");
  }
  f.close();
}

///Get policy from remote URL
void getfromURL(const char* name, std::string& xml_policy){
  Arc::URL url(name);

  // TODO: IMPORTANT: Use client interface.
  Arc::NS ns;
  Arc::Config c(ns);
  Arc::XMLNode cfg = c;
  Arc::XMLNode mgr = cfg.NewChild("ModuleManager");
  Arc::XMLNode pth1 = mgr.NewChild("Path");
  pth1 = "../../../mcc/tcp/.libs";
  Arc::XMLNode pth2 = mgr.NewChild("Path");
  pth2 = "../../../mcc/http/.libs";
  Arc::XMLNode plg1 = cfg.NewChild("Plugins");
  Arc::XMLNode mcctcp = plg1.NewChild("Name");
  mcctcp = "mcctcp";
  Arc::XMLNode plg2 = cfg.NewChild("Plugins");
  Arc::XMLNode mcchttp = plg2.NewChild("Name");
  mcchttp = "mcchttp";
  Arc::XMLNode chn = cfg.NewChild("Chain");

  Arc::XMLNode tcp = chn.NewChild("Component");
  Arc::XMLNode tcpname = tcp.NewAttribute("name");
  tcpname = "tcp.client";
  Arc::XMLNode tcpid = tcp.NewAttribute("id");
  tcpid = "tcp";
  Arc::XMLNode tcpcnt = tcp.NewChild("Connect");
  Arc::XMLNode tcphost = tcpcnt.NewChild("Host");
  tcphost = url.Host();
  Arc::XMLNode tcpport = tcpcnt.NewChild("Port");
  tcpport = Arc::tostring(url.Port());

  Arc::XMLNode http = chn.NewChild("Component");
  Arc::XMLNode httpname = http.NewAttribute("name");
  httpname = "http.client";
  Arc::XMLNode httpid = http.NewAttribute("id");
  httpid = "http";
  Arc::XMLNode httpentry = http.NewAttribute("entry");
  httpentry = "http";
  Arc::XMLNode httpnext = http.NewChild("next");
  Arc::XMLNode httpnextid = httpnext.NewAttribute("id");
  httpnextid = "tcp";
  Arc::XMLNode httpmeth = http.NewChild("Method");
  httpmeth = "GET";
  Arc::XMLNode httpep = http.NewChild("Endpoint");
  httpep = url.str();

  //std::cout<<"------ Configuration ------"<<std::endl;
  //std::string cfgstr;
  //c.GetDoc(cfgstr);
  //std::cerr << cfgstr << std::endl;

  Arc::Loader l(&c);

  Arc::Message request;
  Arc::PayloadRaw msg;
  Arc::MessageAttributes attributes;
  Arc::MessageContext context;
  request.Payload(&msg);
  request.Attributes(&attributes);
  request.Context(&context);
  Arc::Message response;

  if(!(l["http"]->process(request,response)))
    logger.msg(ERROR,"Failed to read policy from URL %s",name);

  try {
    Arc::PayloadRaw& payload =
      dynamic_cast<Arc::PayloadRaw&>(*response.Payload());
    xml_policy.append(payload.Content());
    xml_policy.append(" ");
  } catch(std::exception&) { };
}
*/

Policy* PolicyParser::parsePolicy(const Source& source, std::string policyclassname, EvaluatorContext* ctx){
  Arc::XMLNode node = source.Get();  
  Arc::ClassLoader* classloader = NULL;
  classloader=Arc::ClassLoader::getClassLoader();
  ArcSec::Policy * policy = (ArcSec::Policy*)(classloader->Instance(policyclassname, &node));
  if(policy == NULL) { logger.msg(ERROR, "Can not generate policy object"); return NULL; }
  policy->setEvaluatorContext(ctx);
  policy->make_policy();

  return policy;
}

