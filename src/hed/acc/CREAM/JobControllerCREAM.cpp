// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/URL.h>
#include <arc/UserConfig.h>
#include <arc/message/MCC.h>

#include "CREAMClient.h"
#include "JobControllerCREAM.h"

namespace Arc {

  Logger JobControllerCREAM::logger(Logger::getRootLogger(), "JobController.CREAM");

  JobControllerCREAM::JobControllerCREAM(const UserConfig& usercfg)
    : JobController(usercfg, "CREAM") {}

  JobControllerCREAM::~JobControllerCREAM() {}

  Plugin* JobControllerCREAM::Instance(PluginArgument *arg) {
    JobControllerPluginArgument *jcarg =
      dynamic_cast<JobControllerPluginArgument*>(arg);
    if (!jcarg)
      return NULL;
    return new JobControllerCREAM(*jcarg);
  }

  void JobControllerCREAM::GetJobInformation() {

    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    for (std::list<Job>::iterator iter = jobstore.begin();
         iter != jobstore.end(); iter++) {
      PathIterator pi(iter->JobID.Path(), true);
      URL url(iter->JobID);
      url.ChangePath(*pi);
      CREAMClient gLiteClient(url, cfg, usercfg.Timeout());
      if (!gLiteClient.stat(pi.Rest(), (*iter))) {
        logger.msg(WARNING, "Job information not found: %s", iter->JobID.fullstr());
      }
    }
  }

  bool JobControllerCREAM::RetrieveJob(const Job& job,
                                       const std::string& downloaddir,
                                       bool usejobname,
                                       bool force) {
    logger.msg(VERBOSE, "Downloading job: %s", job.JobID.str());

    std::string jobidnum;
    if (usejobname && !job.Name.empty())
      jobidnum = job.Name;
    else {
      std::string path = job.JobID.Path();
      std::string::size_type pos = path.rfind('/');
      jobidnum = path.substr(pos + 1);
    }

    std::list<std::string> files;
    if (!ListFilesRecursive(job.OSB, files)) {
      logger.msg(ERROR, "Unable to retrieve list of job files to download for job %s", job.JobID.fullstr());
      return false;
    }

    URL src(job.OSB);
    URL dst(downloaddir.empty() ? jobidnum : downloaddir + G_DIR_SEPARATOR_S + jobidnum);

    std::string srcpath = src.Path();
    std::string dstpath = dst.Path();

    if (!force && Glib::file_test(dstpath, Glib::FILE_TEST_EXISTS)) {
      logger.msg(WARNING, "%s directory exist! Skipping job.", dstpath);
      return false;
    }

    if (srcpath.empty() || (srcpath[srcpath.size() - 1] != '/'))
      srcpath += '/';
    if (dstpath.empty() || (dstpath[dstpath.size() - 1] != G_DIR_SEPARATOR))
      dstpath += G_DIR_SEPARATOR_S;

    bool ok = true;

    for (std::list<std::string>::iterator it = files.begin();
         it != files.end(); it++) {
      src.ChangePath(srcpath + *it);
      dst.ChangePath(dstpath + *it);
      if (!CopyJobFile(src, dst)) {
        logger.msg(INFO, "Failed dowloading %s to %s", src.str(), dst.str());
        ok = false;
      }
    }

    return ok;
  }

  bool JobControllerCREAM::CleanJob(const Job& job) {

    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    PathIterator pi(job.JobID.Path(), true);
    URL url(job.JobID);
    url.ChangePath(*pi);
    CREAMClient gLiteClient(url, cfg, usercfg.Timeout());
    if (!gLiteClient.purge(pi.Rest())) {
      logger.msg(INFO, "Failed cleaning job: %s", job.JobID.str());
      return false;
    }
    PathIterator pi2(job.InfoEndpoint.Path(), true);
    URL url2(job.InfoEndpoint);
    url2.ChangePath(*pi2);
    CREAMClient gLiteClient2(url2, cfg, usercfg.Timeout());
    if (!gLiteClient2.destroyDelegation(pi2.Rest())) {
      logger.msg(INFO, "Failed destroying delegation credentials for job: %s", job.JobID.str());
      return false;
    }
    return true;
  }

  bool JobControllerCREAM::CancelJob(const Job& job) {

    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    PathIterator pi(job.JobID.Path(), true);
    URL url(job.JobID);
    url.ChangePath(*pi);
    CREAMClient gLiteClient(url, cfg, usercfg.Timeout());
    if (!gLiteClient.cancel(pi.Rest())) {
      logger.msg(INFO, "Failed canceling job: %s", job.JobID.str());
      return false;
    }
    return true;
  }

  bool JobControllerCREAM::RenewJob(const Job& /* job */) {
    logger.msg(INFO, "Renewal of CREAM jobs is not supported");
    return false;
  }

  bool JobControllerCREAM::ResumeJob(const Job& /* job */) {
    logger.msg(INFO, "Resumation of CREAM jobs is not supported");
    return false;
  }

  URL JobControllerCREAM::GetFileUrlForJob(const Job& /* job */,
                                           const std::string& /* whichfile */) const {
    return URL();
  }

  bool JobControllerCREAM::GetJobDescription(const Job& /* job */, std::string& /* desc_str */) {
    return false;
  }

  URL JobControllerCREAM::CreateURL(std::string service, ServiceType /* st */) {
    std::string::size_type pos1 = service.find("://");
    if (pos1 == std::string::npos) {
      service = "ldap://" + service;
      pos1 = 4;
    }
    std::string::size_type pos2 = service.find(":", pos1 + 3);
    std::string::size_type pos3 = service.find("/", pos1 + 3);
    if (pos3 == std::string::npos) {
      if (pos2 == std::string::npos)
        service += ":2170";
      // Is this a good default path?
      // Different for computing and index?
      service += "/o=Grid";
    }
    else if (pos2 == std::string::npos || pos2 > pos3)
      service.insert(pos3, ":2170");
    return service;
  }

} // namespace Arc
