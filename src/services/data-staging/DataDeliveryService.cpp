#include <sys/stat.h>

#include <arc/message/MessageAttributes.h>
#include <arc/message/PayloadRaw.h>
#include <arc/message/PayloadStream.h>
#include <arc/message/PayloadSOAP.h>

#include <arc/GUID.h>
#include <arc/FileUtils.h>
#include <arc/Utils.h>

#include "DataDeliveryService.h"

namespace DataStaging {

  static Arc::Plugin *get_service(Arc::PluginArgument* arg)
  {
    Arc::ServicePluginArgument* srvarg =
            arg?dynamic_cast<Arc::ServicePluginArgument*>(arg):NULL;
    if(!srvarg) return NULL;
    DataDeliveryService* s = new DataDeliveryService((Arc::Config*)(*srvarg));
    if (*s)
      return s;
    delete s;
    return NULL;
  }

  Arc::Logger DataDeliveryService::logger(Arc::Logger::rootLogger, "DataDeliveryService");


  void DataDeliveryService::ArchivalThread(void* arg) {
    DataDeliveryService* service = (DataDeliveryService*)arg;
    service->ArchivalThread();
  }

  void DataDeliveryService::ArchivalThread() {
    // archive every 10 mins DTRs older than 1 hour
    // TODO: configurable, save to disk?
    int frequency = 600;

    while (true) {
      sleep(frequency);
      Arc::Time timelimit(Arc::Time()-Arc::Period(3600));

      active_dtrs_lock.lock();
      for (std::map<DTR*, std::stringstream*>::iterator i = active_dtrs.begin();
           i != active_dtrs.end(); ++i) {

        DTR* dtr = i->first;

        if (dtr->get_modification_time() < timelimit && dtr->get_status() != DTRStatus::TRANSFERRING) {
          archived_dtrs_lock.lock();
          if (dtr->error()) {
            logger.msg(Arc::VERBOSE, "Archiving DTR %s, state ERROR", dtr->get_id());
            archived_dtrs[dtr->get_id()] = std::pair<std::string, std::string>("ERROR", dtr->get_error_status().GetDesc());
          }
          else {
            logger.msg(Arc::VERBOSE, "Archiving DTR %s, state %s", dtr->get_id(), dtr->get_status().str());
            archived_dtrs[dtr->get_id()] = std::pair<std::string, std::string>(dtr->get_status().str(), "");
          }
          archived_dtrs_lock.unlock();
          // clean up DTR memory - delete DTR Logger and LogDestinations
          cleanDTR(i->first);
          delete i->second;
          active_dtrs.erase(i);
        }
      }
      active_dtrs_lock.unlock();
    }

  }

  void DataDeliveryService::cleanDTR(DTR* dtr) {
    if (!dtr) return;

    if (dtr->get_logger()) {
      dtr->get_logger()->deleteDestinations();
      delete dtr->get_logger();
    }
    delete dtr;
    dtr = NULL;
  }

  bool DataDeliveryService::CheckInput(const std::string& url, const Arc::UserConfig& usercfg, Arc::XMLNode& resultelement) {

    Arc::DataHandle h(url, usercfg);
    if (!h || !(*h)) {
      resultelement.NewChild("ErrorDescription") = "Can't handle URL " + url;
      return false;
    }
    if (h->Local()) {
      std::string path(h->GetURL().Path());
      if (path.find("../") != std::string::npos) {
        resultelement.NewChild("ErrorDescription") = "'../' is not allowed in filename";
        return false;
      }
      bool allowed = false;
      for (std::list<std::string>::iterator i = allowed_dirs.begin(); i != allowed_dirs.end(); ++i) {
        if (path.find(*i) == 0) allowed = true;
      }
      if (!allowed) {
        resultelement.NewChild("ErrorDescription") = "Access denied to path " + path;
        return false;
      }
    }
    return true;
  }

  void DataDeliveryService::receiveDTR(DTR& dtr) {
    // note: logger doesn't work here - to fix
    logger.msg(Arc::INFO, "Received DTR %s in state %s", dtr.get_id(), dtr.get_status().str());

    // delete temp proxy file
    std::string proxy_file(tmp_proxy_dir+"/DTR."+dtr.get_id()+".proxy");
    logger.msg(Arc::DEBUG, "Removing temp proxy %s", proxy_file);
    if (unlink(proxy_file.c_str()) && errno != ENOENT) {
      logger.msg(Arc::WARNING, "Failed to remove temporary proxy %s: %s", proxy_file, Arc::StrError(errno));
    }
    --current_processes;
  }

  /*
   Accepts:
   <DataDeliveryStart>
     <DTR>
       <ID>id</ID>
       <Source>url</Source>
       <Destination>url</Destination>
       <Uid>1000</Uid>
       <Gid>1000</Gid>
       <Caching>true</Caching>
       <CheckSum>adler32:12345678</CheckSum>
       <MinAverageSpeed>100</MinAverageSpeed>
       <AverageTime>60</AverageTime>
       <MinCurrentSpeed>100</MinCurrentSpeed>
       <MaxInactivityTime>120</MaxInactivityTime>
     </DTR>
     <DTR>
      ...
   </DataDeliveryStart>

   Returns
   <DataDeliveryStartResponse>
     <DataDeliveryStartResult>
       <Result>
         <ID>id</ID>
         <ReturnCode>SERVICE_ERROR</ReturnCode>
         <ErrorDescription>...</ErrorDescription>
       </Result>
       ...
     </DataDeliveryStartResult>
   </DataDeliveryStartResponse>
   */
  Arc::MCC_Status DataDeliveryService::Start(Arc::XMLNode in, Arc::XMLNode out) {

    Arc::XMLNode resp = out.NewChild("DataDeliveryStartResponse");
    Arc::XMLNode results = resp.NewChild("DataDeliveryStartResult");

    // Save credentials to temp file and set in UserConfig
    Arc::XMLNode delegated_token = in["DataDeliveryStart"]["deleg:DelegatedToken"];
    if (!delegated_token) {
      logger.msg(Arc::ERROR, "No delegation token in request");
      return Arc::MCC_Status(Arc::GENERIC_ERROR, "DataDeliveryService", "No delegation token received");
    }

    // Check credentials were already delegated
    std::string credential;
    if (!delegation.DelegatedToken(credential, delegated_token)) {
      // Failed to accept delegation
      logger.msg(Arc::ERROR, "Failed to accept delegation");
      return Arc::MCC_Status(Arc::GENERIC_ERROR, "DataDeliveryService", "Failed to accept delegation");
    }

    for(int n = 0;;++n) {
      Arc::XMLNode dtrnode = in["DataDeliveryStart"]["DTR"][n];

      if (!dtrnode) break;

      std::string dtrid((std::string)dtrnode["ID"]);
      std::string src((std::string)dtrnode["Source"]);
      std::string dest((std::string)dtrnode["Destination"]);
      int uid = Arc::stringtoi((std::string)dtrnode["Uid"]);
      int gid = Arc::stringtoi((std::string)dtrnode["Gid"]);
      if (dtrnode["Caching"] == "true") {
        uid = Arc::User().get_uid();
        gid = Arc::User().get_gid();
      }
      // proxy path will be set later
      Arc::UserConfig usercfg(Arc::initializeCredentialsType(Arc::initializeCredentialsType::SkipCredentials));

      Arc::XMLNode resultelement = results.NewChild("Result");
      resultelement.NewChild("ID") = dtrid;

      if (!CheckInput(src, usercfg, resultelement)) {
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement["ErrorDescription"] = (std::string)resultelement["ErrorDescription"] + ": Cannot use source";
        logger.msg(Arc::ERROR, (std::string)resultelement["ErrorDescription"]);
        continue;
      }

      if (!CheckInput(dest, usercfg, resultelement)) {
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement["ErrorDescription"] = (std::string)resultelement["ErrorDescription"] + ": Cannot use destination";
        logger.msg(Arc::ERROR, (std::string)resultelement["ErrorDescription"]);
       continue;
      }

      if (current_processes >= max_processes) {
        logger.msg(Arc::WARNING, "All %u process slots used", max_processes);
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement.NewChild("ErrorDescription") = "No free process slot available";
       continue;
      }

      // check if dtrid is in the active list - if so it is probably a retry
      active_dtrs_lock.lock();
      std::map<DTR*, std::stringstream*>::iterator i = active_dtrs.begin();

      for (; i != active_dtrs.end(); ++i) {
        if (i->first->get_id() == dtrid) break;
      }
      if (i != active_dtrs.end()) {
        if (i->first->get_status() == DTRStatus::TRANSFERRING) {
          logger.msg(Arc::ERROR, "Received retry for DTR %s still in transfer", dtrid);
          resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
          resultelement.NewChild("ErrorDescription") = "DTR is still in transfer";
          active_dtrs_lock.unlock();
          continue;
        }
        // Erase this DTR from active list
        logger.msg(Arc::VERBOSE, "Replacing DTR %s in state %s with new request", dtrid, i->first->get_status().str());
        cleanDTR(i->first);
        delete i->second;
        active_dtrs.erase(i);
      }
      active_dtrs_lock.unlock();

      // Store proxy, only readable by user. Use DTR job id as proxy name.
      // TODO: it is inefficient to create a file for every DTR, better to
      // use some kind of proxy store
      std::string proxy_file(tmp_proxy_dir+"/DTR."+dtrid+".proxy");
      logger.msg(Arc::VERBOSE, "Storing temp proxy at %s", proxy_file);

      if (!Arc::FileCreate(proxy_file, credential)) {
        logger.msg(Arc::ERROR, "Failed to create temp proxy at %s: %s", proxy_file, Arc::StrError(errno));
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement.NewChild("ErrorDescription") = "Failed to store temporary proxy";
        continue;
      }

      if (chown(proxy_file.c_str(), uid, gid) != 0) {
        logger.msg(Arc::ERROR, "Failed to change owner of temp proxy at %s to %i:%i: %s",
                   proxy_file, uid, gid, Arc::StrError(errno));
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement.NewChild("ErrorDescription") = "Failed to store temporary proxy";
        continue;
      }
      usercfg.ProxyPath(proxy_file);

      // Logger for this DTR. Uses a string stream so log can easily be sent
      // back to the client. LogStream keeps a reference to the stream so we
      // cannot delete it until deleting LogStream. These pointers are
      // deleted when the DTR is archived.
      std::stringstream * stream = new std::stringstream();
      Arc::LogDestination * output = new Arc::LogStream(*stream);
      Arc::Logger * log = new Arc::Logger(Arc::Logger::getRootLogger(), "DataStaging");
      log->removeDestinations();
      log->addDestination(*output);

      std::string groupid(Arc::UUID());

      DTR * dtr = new DTR(src, dest, usercfg, groupid, uid, log);
      if (!(*dtr)) {
        logger.msg(Arc::ERROR, "Invalid DTR");
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement.NewChild("ErrorDescription") = "Could not create DTR";
        cleanDTR(dtr);
        if (unlink(proxy_file.c_str()) && errno != ENOENT) {
          logger.msg(Arc::WARNING, "Failed to remove temporary proxy %s: %s", proxy_file, Arc::StrError(errno));
        }
        continue;
      }
      ++current_processes;

      // Set source checksum to validate against
      if (dtrnode["CheckSum"]) dtr->get_source()->SetCheckSum((std::string)dtrnode["CheckSum"]);

      // Get the callbacks sent to Scheduler and connect Delivery
      dtr->registerCallback(this, SCHEDULER);
      dtr->registerCallback(&delivery, DELIVERY);

      // Set transfer limits
      TransferParameters transfer_params;
      if (dtrnode["MinAverageSpeed"]) transfer_params.min_average_bandwidth = Arc::stringtoull((std::string)dtrnode["MinAverageSpeed"]);
      if (dtrnode["AverageTime"]) transfer_params.averaging_time = Arc::stringtoui((std::string)dtrnode["AverageTime"]);
      if (dtrnode["MinCurrentSpeed"]) transfer_params.min_current_bandwidth = Arc::stringtoull((std::string)dtrnode["MinCurrentSpeed"]);
      if (dtrnode["MaxInactivityTime"]) transfer_params.max_inactivity_time = Arc::stringtoui((std::string)dtrnode["MaxInactivityTime"]);
      delivery.SetTransferParameters(transfer_params);

      dtr->set_id(dtrid);
      dtr->set_status(DTRStatus::TRANSFER);

      dtr->push(DELIVERY);

      // Add to active list
      active_dtrs_lock.lock();
      active_dtrs[dtr] = stream;
      active_dtrs_lock.unlock();

      resultelement.NewChild("ResultCode") = "OK";
    }
    return Arc::MCC_Status(Arc::STATUS_OK);
  }

  /*
   Accepts:
   <DataDeliveryQuery>
     <DTR>
       <ID>id</ID>
     </DTR>
     <DTR>
     ...
   </DataDeliveryQuery>

   Returns:
   <DataDeliveryQueryResponse>
     <DataDeliveryQueryResult>
       <Result>
         <ID>id</ID>
         <ReturnCode>ERROR</ReturnCode>
         <ErrorDescription>...</ErrorDescription>
         <ErrorStatus>2</ErrorStatus>
         <ErrorLocation>1</ErrorLocation>
         <Log>...</Log>
         <BytesTransferred>1234</BytesTransferred>
         <Checksum>adler32:a123a45</Checksum>
       </Result>
       ...
     </DataDeliveryQueryResult>
   </DataDeliveryQueryResponse>
   */
  Arc::MCC_Status DataDeliveryService::Query(Arc::XMLNode in, Arc::XMLNode out) {

    Arc::XMLNode resp = out.NewChild("DataDeliveryQueryResponse");
    Arc::XMLNode results = resp.NewChild("DataDeliveryQueryResult");

    for(int n = 0;;++n) {
      Arc::XMLNode dtrnode = in["DataDeliveryQuery"]["DTR"][n];

      if (!dtrnode) break;

      std::string dtrid((std::string)dtrnode["ID"]);

      Arc::XMLNode resultelement = results.NewChild("Result");
      resultelement.NewChild("ID") = dtrid;

      active_dtrs_lock.lock();
      std::map<DTR*, std::stringstream*>::iterator dtr_it = active_dtrs.begin();
      for (; dtr_it != active_dtrs.end(); ++dtr_it) {
        if (dtr_it->first->get_id() == dtrid) break;
      }

      if (dtr_it == active_dtrs.end()) {
        active_dtrs_lock.unlock();

        // if not in active list, look in archived list
        archived_dtrs_lock.lock();
        std::map<std::string, std::pair<std::string, std::string> >::const_iterator arc_it = archived_dtrs.find(dtrid);
        if (arc_it != archived_dtrs.end()) {
          resultelement.NewChild("ResultCode") = archived_dtrs[dtrid].first;
          resultelement.NewChild("ErrorDescription") = archived_dtrs[dtrid].second;
          archived_dtrs_lock.unlock();
          continue;
        }
        archived_dtrs_lock.unlock();

        logger.msg(Arc::ERROR, "No such DTR %s", dtrid);
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement.NewChild("ErrorStatus") = Arc::tostring(DTRErrorStatus::ERROR_TRANSFER);
        resultelement.NewChild("ErrorDescription") = "No such DTR";
        continue;
      }

      DTR * dtr = dtr_it->first;
      resultelement.NewChild("Log") = dtr_it->second->str();
      resultelement.NewChild("BytesTransferred") = Arc::tostring(dtr->get_bytes_transferred());

      if (dtr->error()) {
        logger.msg(Arc::INFO, "DTR %s failed", dtrid);
        resultelement.NewChild("ResultCode") = "TRANSFER_ERROR";
        resultelement.NewChild("ErrorDescription") = dtr->get_error_status().GetDesc();
        resultelement.NewChild("ErrorStatus") = Arc::tostring(dtr->get_error_status().GetErrorStatus());
        resultelement.NewChild("ErrorLocation") = Arc::tostring(dtr->get_error_status().GetErrorLocation());
        archived_dtrs_lock.lock();
        archived_dtrs[dtrid] = std::pair<std::string, std::string>("ERROR", dtr->get_error_status().GetDesc());
        archived_dtrs_lock.unlock();
      }
      else if (dtr->get_status() == DTRStatus::TRANSFERRED) {
        logger.msg(Arc::INFO, "DTR %s finished successfully", dtrid);
        resultelement.NewChild("ResultCode") = "TRANSFERRED";
        // pass calculated checksum back to Scheduler (eg to insert in catalog)
        if (dtr->get_destination()->CheckCheckSum()) resultelement.NewChild("CheckSum") = dtr->get_destination()->GetCheckSum();
        archived_dtrs_lock.lock();
        archived_dtrs[dtrid] = std::pair<std::string, std::string>("TRANSFERRED", "");
        archived_dtrs_lock.unlock();
      }
      else {
        logger.msg(Arc::INFO, "DTR %s still in progress (%lluB transferred)",
                   dtrid, dtr->get_bytes_transferred());
        resultelement.NewChild("ResultCode") = "TRANSFERRING";
        active_dtrs_lock.unlock();
        return Arc::MCC_Status(Arc::STATUS_OK);
      }
      // Terminal state -  clean up DTR memory and delete DTR Logger and LogDestinations
      cleanDTR(dtr);
      delete dtr_it->second;
      active_dtrs.erase(dtr_it);
      active_dtrs_lock.unlock();
    }
    return Arc::MCC_Status(Arc::STATUS_OK);
  }

  /*
   Accepts:
   <DataDeliveryCancel>
     <DTR>
       <ID>id</ID>
     </DTR>
     <DTR>
     ...
   </DataDeliveryCancel>

   Returns:
   <DataDeliveryCancelResponse>
     <DataDeliveryCancelResult>
       <Result>
         <ID>id</ID>
         <ReturnCode>ERROR</ReturnCode>
         <ErrorDescription>...</ErrorDescription>
       </Result>
       ...
     </DataDeliveryCancelResult>
   </DataDeliveryCancelResponse>
   */
  Arc::MCC_Status DataDeliveryService::Cancel(Arc::XMLNode in, Arc::XMLNode out) {

    Arc::XMLNode resp = out.NewChild("DataDeliveryCancelResponse");
    Arc::XMLNode results = resp.NewChild("DataDeliveryCancelResult");

    for (int n = 0;;++n) {
      Arc::XMLNode dtrnode = in["DataDeliveryCancel"]["DTR"][n];

      if (!dtrnode) break;

      std::string dtrid((std::string)dtrnode["ID"]);

      Arc::XMLNode resultelement = results.NewChild("Result");
      resultelement.NewChild("ID") = dtrid;

      // Check if DTR is still in active list
      active_dtrs_lock.lock();
      std::map<DTR*, std::stringstream*>::iterator dtr_it = active_dtrs.begin();
      for (; dtr_it != active_dtrs.end(); ++dtr_it) {
        if (dtr_it->first->get_id() == dtrid) break;
      }

      if (dtr_it == active_dtrs.end()) {
        active_dtrs_lock.unlock();
        logger.msg(Arc::ERROR, "No active DTR %s", dtrid);
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement.NewChild("ErrorDescription") = "No such active DTR";
        continue;
      }
      // DTR could be already finished, but report successful cancel anyway

      DTR * dtr = dtr_it->first;
      if (dtr->get_status() == DTRStatus::TRANSFERRING_CANCEL) {
        active_dtrs_lock.unlock();
        logger.msg(Arc::ERROR, "DTR %s was already cancelled", dtrid);
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement.NewChild("ErrorDescription") = "DTR already cancelled";
        continue;
      }

      // Delivery will automatically kill running process
      if (!delivery.cancelDTR(dtr)) {
        active_dtrs_lock.unlock();
        logger.msg(Arc::ERROR, "DTR %s could not be cancelled", dtrid);
        resultelement.NewChild("ResultCode") = "SERVICE_ERROR";
        resultelement.NewChild("ErrorDescription") = "DTR could not be cancelled";
        continue;
      }

      logger.msg(Arc::INFO, "DTR %s cancelled", dtr->get_id());
      resultelement.NewChild("ResultCode") = "OK";
      active_dtrs_lock.unlock();
    }

    return Arc::MCC_Status(Arc::STATUS_OK);
  }

  /*
   Accepts:
   <DataDeliveryPing/>

   Returns:
   <DataDeliveryPingResponse>
     <DataDeliveryPingResult>
       <Result>
         <ReturnCode>ERROR</ReturnCode>
         <ErrorDescription>...</ErrorDescription>
         <AllowedDir>/var/arc/cache</AllowedDir>
         ...
       </Result>
       ...
     </DataDeliveryPingResult>
   </DataDeliveryPingResponse>
   */
  Arc::MCC_Status DataDeliveryService::Ping(Arc::XMLNode in, Arc::XMLNode out) {

    Arc::XMLNode resultelement = out.NewChild("DataDeliveryPingResponse").NewChild("DataDeliveryPingResult").NewChild("Result");
    resultelement.NewChild("ResultCode") = "OK";

    for (std::list<std::string>::iterator dir = allowed_dirs.begin(); dir != allowed_dirs.end(); ++dir) {
      resultelement.NewChild("AllowedDir") = *dir;
    }

    return Arc::MCC_Status(Arc::STATUS_OK);
  }


  DataDeliveryService::DataDeliveryService(Arc::Config *cfg)
    : RegisteredService(cfg),
      max_processes(100),
      current_processes(0) {

    valid = false;
    // Check configuration - at least one allowed IP address and dir must be specified
    if (!(*cfg)["SecHandler"]["PDP"]["Policy"]["Rule"]["Subjects"]["Subject"]["SubjectAttribute"]) {
      logger.msg(Arc::ERROR, "Invalid configuration - no allowed IP address specified");
      return;
    }
    if (!(*cfg)["AllowedDir"]) {
      logger.msg(Arc::ERROR, "Invalid configuration - no allowed dirs specified");
      return;
    }
    for (int n = 0;;++n) {
      Arc::XMLNode allowed_dir = (*cfg)["AllowedDir"][n];
      if (!allowed_dir) break;
      allowed_dirs.push_back((std::string)allowed_dir);
    }

    // Start archival thread
    if (!Arc::CreateThreadFunction(ArchivalThread, this)) {
      logger.msg(Arc::ERROR, "Failed to start archival thread");
      return;
    }
    // Create tmp dir for proxies
    // TODO get from configuration
    tmp_proxy_dir = "/tmp/arc";

    // clear any proxies left behind from previous bad shutdown
    Arc::DirDelete(tmp_proxy_dir);
    if (!Arc::DirCreate(tmp_proxy_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true)) {
      logger.msg(Arc::ERROR, "Failed to create dir %s for temp proxies: %s", tmp_proxy_dir, Arc::StrError(errno));
      return;
    }
    // Set restrictive umask
    umask(0077);
    // Start new DataDelivery
    delivery.start();
    valid = true;
  }

  DataDeliveryService::~DataDeliveryService() {
    // Stop accepting new requests and cancel all active transfers
    // DataDelivery destructor automatically calls stop()
    valid = false;
    // clear any proxies left behind
    Arc::DirDelete(tmp_proxy_dir);
    logger.msg(Arc::INFO, "Shutting down data delivery service");
  }

  Arc::MCC_Status DataDeliveryService::process(Arc::Message &inmsg, Arc::Message &outmsg) {

    if (!valid) return make_soap_fault(outmsg, "Service is not valid");

    // Check authorization
    if(!ProcessSecHandlers(inmsg, "incoming")) {
      logger.msg(Arc::ERROR, "Unauthorized");
      return make_soap_fault(outmsg, "Authorization failed");
    }
    std::string method = inmsg.Attributes()->get("HTTP:METHOD");

    if(method == "POST") {
      logger.msg(Arc::VERBOSE, "process: POST");
      logger.msg(Arc::VERBOSE, "Identity is %s", inmsg.Attributes()->get("TLS:PEERDN"));
      // Both input and output are supposed to be SOAP
      // Extracting payload
      Arc::PayloadSOAP* inpayload = NULL;
      try {
        inpayload = dynamic_cast<Arc::PayloadSOAP*>(inmsg.Payload());
      } catch(std::exception& e) { };
      if(!inpayload) {
        logger.msg(Arc::ERROR, "input is not SOAP");
        return make_soap_fault(outmsg);
      }
      // Applying known namespaces
      inpayload->Namespaces(ns);
      if(logger.getThreshold() <= Arc::DEBUG) {
          std::string str;
          inpayload->GetDoc(str, true);
          logger.msg(Arc::DEBUG, "process: request=%s",str);
      }
      // Analyzing request
      Arc::XMLNode op = inpayload->Child(0);
      if(!op) {
        logger.msg(Arc::ERROR, "input does not define operation");
        return make_soap_fault(outmsg);
      }
      logger.msg(Arc::VERBOSE, "process: operation: %s",op.Name());

      Arc::PayloadSOAP* outpayload = new Arc::PayloadSOAP(ns);
      outpayload->Namespaces(ns);

      Arc::MCC_Status result(Arc::STATUS_OK);
      // choose operation
      // Make a new request
      if (MatchXMLName(op,"DataDeliveryStart")) {
        result = Start(*inpayload, *outpayload);
      }
      // Query a request
      else if (MatchXMLName(op,"DataDeliveryQuery")) {
        result = Query(*inpayload, *outpayload);
      }
      // Cancel a request
      else if (MatchXMLName(op,"DataDeliveryCancel")) {
        result = Cancel(*inpayload, *outpayload);
      }
      // ping service
      else if (MatchXMLName(op,"DataDeliveryPing")) {
        result = Ping(*inpayload, *outpayload);
      }
      // Delegate credentials. Should be called before making a new request
      else if (delegation.MatchNamespace(*inpayload)) {
        if (!delegation.Process(*inpayload, *outpayload)) {
          delete outpayload;
          return make_soap_fault(outmsg);
        }
      }
      // Unknown operation
      else {
        logger.msg(Arc::ERROR, "SOAP operation is not supported: %s", op.Name());
        delete outpayload;
        return make_soap_fault(outmsg);
      }

      if (!result)
        return make_soap_fault(outmsg, result.getExplanation());

      if (logger.getThreshold() <= Arc::DEBUG) {
        std::string str;
        outpayload->GetDoc(str, true);
        logger.msg(Arc::DEBUG, "process: response=%s", str);
      }
      outmsg.Payload(outpayload);

      if (!ProcessSecHandlers(outmsg,"outgoing")) {
        logger.msg(Arc::ERROR, "Security Handlers processing failed");
        delete outmsg.Payload(NULL);
        return Arc::MCC_Status();
      }
    }
    else {
      // only POST supported
      logger.msg(Arc::ERROR, "Only POST is supported in DataDeliveryService");
      return Arc::MCC_Status();
    }
    return Arc::MCC_Status(Arc::STATUS_OK);
  }

  bool DataDeliveryService::RegistrationCollector(Arc::XMLNode &doc) {
    Arc::NS isis_ns; isis_ns["isis"] = "http://www.nordugrid.org/schemas/isis/2008/08";
    Arc::XMLNode regentry(isis_ns, "RegEntry");
    regentry.NewChild("SrcAdv").NewChild("Type") = "org.nordugrid.execution.datadeliveryservice";
    regentry.New(doc);
    return true;
  }

  Arc::MCC_Status DataDeliveryService::make_soap_fault(Arc::Message& outmsg, const std::string& reason) {
    Arc::PayloadSOAP* outpayload = new Arc::PayloadSOAP(ns,true);
    Arc::SOAPFault* fault = outpayload?outpayload->Fault():NULL;
    if(fault) {
      fault->Code(Arc::SOAPFault::Sender);
      if (reason.empty())
        fault->Reason("Failed processing request");
      else
        fault->Reason("Failed processing request: "+reason);
    }
    outmsg.Payload(outpayload);
    return Arc::MCC_Status(Arc::STATUS_OK);
  }

} // namespace DataStaging

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
    { "datadeliveryservice", "HED:SERVICE", NULL, 0, &DataStaging::get_service },
    { NULL, NULL, NULL, 0, NULL }
};

