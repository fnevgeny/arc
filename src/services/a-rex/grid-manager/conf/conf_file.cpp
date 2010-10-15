#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <pwd.h>

#include <arc/StringConv.h>
#include <arc/Utils.h>
#include <arc/XMLNode.h>
#include "../jobs/users.h"
#include "../jobs/states.h"
#include "../jobs/plugins.h"
#include "conf.h"
#include "conf_sections.h"
#include "environment.h"
#include "gridmap.h"
#include "../run/run_plugin.h"
#include "../misc/escaped.h"
#include "../log/job_log.h"
#include "conf_cache.h"
#include "conf_file.h"

static Arc::Logger& logger = Arc::Logger::getRootLogger();

//JobLog job_log;
ContinuationPlugins plugins;
RunPlugin cred_plugin;

static void check_lrms_backends(const std::string& default_lrms,GMEnvironment& env) {
  std::string tool_path;
  tool_path=env.nordugrid_libexec_loc()+"/cancel-"+default_lrms+"-job";
  if(!Glib::file_test(tool_path,Glib::FILE_TEST_IS_REGULAR)) {
    logger.msg(Arc::WARNING,"Missing cancel-%s-job - job cancelation may not work",default_lrms);
  };
  tool_path=env.nordugrid_libexec_loc()+"/submit-"+default_lrms+"-job";
  if(!Glib::file_test(tool_path,Glib::FILE_TEST_IS_REGULAR)) {
    logger.msg(Arc::WARNING,"Missing submit-%s-job - job submission to LRMS may not work",default_lrms);
  };
  tool_path=env.nordugrid_libexec_loc()+"/scan-"+default_lrms+"-job";
  if(!Glib::file_test(tool_path,Glib::FILE_TEST_IS_REGULAR)) {
    logger.msg(Arc::WARNING,"Missing scan-%s-job - may miss when job finished executing",default_lrms);
  };
}

bool configure_serviced_users(JobUsers &users,uid_t my_uid,const std::string &my_username,JobUser &my_user/*,Daemon* daemon*/) {
  std::ifstream cfile;
  std::vector<std::string> session_roots;
  std::string session_root("");
  std::string default_lrms("");
  std::string default_queue("");
  std::string last_control_dir("");
  time_t default_ttl = DEFAULT_KEEP_FINISHED;
  time_t default_ttr = DEFAULT_KEEP_DELETED;
  int default_reruns = DEFAULT_JOB_RERUNS;
  int default_diskspace = DEFAULT_DISKSPACE;
  bool superuser = (my_uid == 0);
  bool strict_session = false;
  std::string central_control_dir("");
  std::string infosys_user("");
  std::string jobreport_key("");
  std::string jobreport_cert("");
  std::string jobreport_cadir("");
  JobsListConfig& jcfg = users.Env().jobs_cfg();

  /* read configuration and add users and other things */
  if(!config_open(cfile,users.Env())) {
    logger.msg(Arc::ERROR,"Can't read configuration file"); return false;
  };
  /* detect type of file */
  switch(config_detect(cfile)) {
    case config_file_XML: {
      Arc::XMLNode cfg;
      if(!cfg.ReadFromStream(cfile)) {
        config_close(cfile);
        logger.msg(Arc::ERROR,"Can't interpret configuration file as XML");
        return false;
      };
      config_close(cfile);
      return configure_serviced_users(cfg,users,my_uid,my_username,my_user);
    }; break;
    case config_file_INI: {
      // Fall through. TODO: make INI processing a separate function.
    }; break;
    default: {
      logger.msg(Arc::ERROR,"Can't recognize type of configuration file"); return false;
    }; break;
  };
  ConfigSections* cf = new ConfigSections(cfile);
  cf->AddSection("common");
  cf->AddSection("grid-manager");
  cf->AddSection("infosys");
  /* process configuration information here */
  for(;;) {
    std::string rest;
    std::string command;
    cf->ReadNext(command,rest);
    if(cf->SectionNum() == 2) { // infosys - looking for user name only
      if(command == "user") infosys_user=rest;
      continue;
    };
    if(cf->SectionNum() == 0) { // infosys user may be in common too
      if(command == "user") infosys_user=rest;
    };
    /*
    if(daemon) {
      int r = daemon->config(command,rest);
      if(r == 0) continue;
      if(r == -1) goto exit;
    } else {
      int r = Daemon::skip_config(command);
      if(r == 0) continue;
    };
    */
    if(command.length() == 0) {
      if(central_control_dir.length() != 0) {
        command="control"; rest=central_control_dir+" .";
        central_control_dir="";
      } else {
        break;
      };
    };
    if(command == "runtimedir") {
      users.Env().runtime_config_dir(rest);
    } else if(command == "joblog") { /* where to write job inforamtion */
      std::string fname = config_next_arg(rest);  /* empty is allowed too */
      users.Env().job_log().SetOutput(fname.c_str());
    }
    else if(command == "jobreport") { /* service to report information to */
      for(;;) {
        std::string url = config_next_arg(rest);
        if(url.length() == 0) break;
        unsigned int i;
        if(Arc::stringto(url,i)) {
          users.Env().job_log().SetExpiration(i);
          continue;
        };
        users.Env().job_log().SetReporter(url.c_str());
      };
    }
    else if(command == "jobreport_credentials") {
      jobreport_key = config_next_arg(rest);
      jobreport_cert = config_next_arg(rest);
      jobreport_cadir = config_next_arg(rest);
    }
    else if(command == "jobreport_options") { /* e.g. for SGAS, interpreted by usage reporter */
      std::string accounting_options = config_next_arg(rest);
      users.Env().job_log().set_options(accounting_options);
    }
    else if(command == "maxjobs") { /* maximum number of the jobs to support */
      std::string max_jobs_s = config_next_arg(rest);
      long int i;
      int max_jobs = -1;
      int max_jobs_running = -1;
      int max_per_dn = -1;
      if(max_jobs_s.length() != 0) {
        if(!Arc::stringto(max_jobs_s,i)) {
          logger.msg(Arc::ERROR,"Wrong number in maxjobs: %s",max_jobs_s);
          goto exit;
        };
        if(i<0) i=-1; max_jobs=i;
      };
      max_jobs_s = config_next_arg(rest);
      if(max_jobs_s.length() != 0) {
        if(!Arc::stringto(max_jobs_s,i)) {
          logger.msg(Arc::ERROR,"Wrong number in maxjobs: %s",max_jobs_s);
          goto exit;
        };
        if(i<0) i=-1; max_jobs_running=i;
      };
      max_jobs_s = config_next_arg(rest);
      if(max_jobs_s.length() != 0) {
        if(!Arc::stringto(max_jobs_s,i)) {
          logger.msg(Arc::ERROR,"Wrong number in maxjobs: %s",max_jobs_s);
          goto exit;
        };
        if(i<0) i=-1; max_per_dn=i;
      }
      jcfg.SetMaxJobs(
              max_jobs,max_jobs_running,max_per_dn);
    }
    else if(command == "maxload") { /* maximum number of the jobs processed on frontend */
      std::string max_jobs_s = config_next_arg(rest);
      long int i;
      int max_jobs_processing, max_jobs_processing_emergency, max_downloads = -1;
      if(max_jobs_s.length() != 0) {
        if(!Arc::stringto(max_jobs_s,i)) {
          logger.msg(Arc::ERROR,"Wrong number in maxload: %s",max_jobs_s);
          goto exit;
        };
        if(i<0) i=-1; max_jobs_processing=i;
      };
      max_jobs_s = config_next_arg(rest);
      if(max_jobs_s.length() != 0) {
        if(!Arc::stringto(max_jobs_s,i)) {
          logger.msg(Arc::ERROR,"Wrong number in maxload: %s",max_jobs_s);
          goto exit;
        };
        if(i<0) i=-1; max_jobs_processing_emergency=i;
      };
      max_jobs_s = config_next_arg(rest);
      if(max_jobs_s.length() != 0) {
        if(!Arc::stringto(max_jobs_s,i)) {
          logger.msg(Arc::ERROR,"Wrong number in maxload: %s",max_jobs_s);
          goto exit;
        };
        if(i<0) i=-1; max_downloads=i;
      };
      jcfg.SetMaxJobsLoad(
              max_jobs_processing,max_jobs_processing_emergency,max_downloads);
    }
    else if(command == "maxloadshare") {
  std::string max_share_s = config_next_arg(rest);
        unsigned int max_share = 0;
        if(max_share_s.length() != 0) {
          if(!Arc::stringto(max_share_s,max_share) || max_share<=0) {
            logger.msg(Arc::ERROR,"Wrong number in maxloadshare: %s", max_share_s); goto exit;
          };
        };
        std::string transfer_share = config_next_arg(rest);
  if (transfer_share.empty()){
            logger.msg(Arc::ERROR,"The type of share is not set in maxloadshare"); goto exit;
  }
        jcfg.SetTransferShare(max_share, transfer_share);
    }
    else if(command == "share_limit") {
      std::string share_name = config_next_arg(rest);
      std::string share_limit_s = config_next_arg(rest);
      std::string temp = config_next_arg(rest);
      while(temp.length() != 0) {
        share_name.append(" ");
        share_name.append(share_limit_s);
        share_limit_s = temp;
        temp = config_next_arg(rest);
      }
      if (share_name.empty()) {
        logger.msg(Arc::ERROR,"The name of share is not set in share_limit"); goto exit;
      }
      unsigned int share_limit = 0;
      if(share_limit_s.length() != 0) {
        if(!Arc::stringto(share_limit_s,share_limit) || share_limit<=0){
          logger.msg(Arc::ERROR,"Wrong number in share_limit: %s",share_limit); goto exit;
        }
      }
      if(!jcfg.AddLimitedShare(share_name,share_limit)) {
        logger.msg(Arc::ERROR,"share_limit should be located after maxloadshare"); goto exit;
      }
    }
    else if(command == "speedcontrol") {
      std::string speed_s = config_next_arg(rest);
      int min_speed=0;
      int min_speed_time=300;
      int min_average_speed=0;
      int max_inactivity_time=300;
      if(speed_s.length() != 0) {
        if(!Arc::stringto(speed_s,min_speed)) {
          logger.msg(Arc::ERROR,"Wrong number in speedcontrol: %s",speed_s);
          goto exit;
        };
      };
      speed_s = config_next_arg(rest);
      if(speed_s.length() != 0) {
        if(!Arc::stringto(speed_s,min_speed_time)) {
          logger.msg(Arc::ERROR,"Wrong number in speedcontrol: ",speed_s);
          goto exit;
        };
      };
      speed_s = config_next_arg(rest);
      if(speed_s.length() != 0) {
        if(!Arc::stringto(speed_s,min_average_speed)) {
          logger.msg(Arc::ERROR,"Wrong number in speedcontrol: %s",speed_s);
          goto exit;
        };
      };
      speed_s = config_next_arg(rest);
      if(speed_s.length() != 0) {
        if(!Arc::stringto(speed_s,max_inactivity_time)) {
          logger.msg(Arc::ERROR,"Wrong number in speedcontrol: %s",speed_s);
          goto exit;
        };
      };
      jcfg.SetSpeedControl(
              min_speed,min_speed_time,min_average_speed,max_inactivity_time);
    }
    else if(command == "wakeupperiod") {
      std::string wakeup_s = config_next_arg(rest);
      unsigned int wakeup_period;
      if(wakeup_s.length() != 0) {
        if(!Arc::stringto(wakeup_s,wakeup_period)) {
          logger.msg(Arc::ERROR,"Wrong number in wakeupperiod: %s",wakeup_s);
          goto exit;
        };
        jcfg.SetWakeupPeriod(wakeup_period);
      };
    }
    else if(command == "securetransfer") {
      std::string s = config_next_arg(rest);
      bool use_secure_transfer;
      if(strcasecmp("yes",s.c_str()) == 0) {
        use_secure_transfer=true;
      }
      else if(strcasecmp("no",s.c_str()) == 0) {
        use_secure_transfer=false;
      }
      else {
        logger.msg(Arc::ERROR,"Wrong option in securetransfer"); goto exit;
      };
      jcfg.SetSecureTransfer(use_secure_transfer);
    }
    else if(command == "passivetransfer") {
      std::string s = config_next_arg(rest);
      bool use_passive_transfer;
      if(strcasecmp("yes",s.c_str()) == 0) {
        use_passive_transfer=true;
      }
      else if(strcasecmp("no",s.c_str()) == 0) {
        use_passive_transfer=false;
      }
      else {
        logger.msg(Arc::ERROR,"Wrong option in passivetransfer"); goto exit;
      };
      jcfg.SetPassiveTransfer(use_passive_transfer);
    }
    else if(command == "maxtransfertries") {
      std::string maxtries_s = config_next_arg(rest);
      int max_retries = DEFAULT_MAX_RETRIES;
      if(maxtries_s.length() != 0) {
        if(!Arc::stringto(maxtries_s,max_retries)) {
          logger.msg(Arc::ERROR,"Wrong number in maxtransfertries"); goto exit;
        };
        jcfg.SetMaxRetries(max_retries);
      };
    }
    else if(command == "norootpower") {
      std::string s = config_next_arg(rest);
      if(strcasecmp("yes",s.c_str()) == 0) {
        strict_session=true;
      }
      else if(strcasecmp("no",s.c_str()) == 0) {
        strict_session=false;
      }
      else {
        logger.msg(Arc::ERROR,"Wrong option in norootpower"); goto exit;
      };
    }
    else if(command == "localtransfer") {
      std::string s = config_next_arg(rest);
      bool use_local_transfer;
      if(strcasecmp("yes",s.c_str()) == 0) {
        use_local_transfer=true;
      }
      else if(strcasecmp("no",s.c_str()) == 0) {
        use_local_transfer=false;
      }
      else {
        logger.msg(Arc::ERROR,"Wrong option in localtransfer"); goto exit;
      };
      jcfg.SetLocalTransfer(use_local_transfer);
    }
    else if(command == "mail") { /* internal address from which to send mail */
      users.Env().support_mail_address(config_next_arg(rest));
      if(users.Env().support_mail_address().empty()) {
        logger.msg(Arc::ERROR,"mail is empty"); goto exit;
      };
    }
    else if(command == "defaultttl") { /* time to keep job after finished */
      char *ep;
      std::string default_ttl_s = config_next_arg(rest);
      if(default_ttl_s.length() == 0) {
        logger.msg(Arc::ERROR,"defaultttl is empty"); goto exit;
      };
      default_ttl=strtoul(default_ttl_s.c_str(),&ep,10);
      if(*ep != 0) {
        logger.msg(Arc::ERROR,"Wrong number in defaultttl command"); goto exit;
      };
      default_ttl_s = config_next_arg(rest);
      if(default_ttl_s.length() != 0) {
        if(rest.length() != 0) {
          logger.msg(Arc::ERROR,"Junk in defaultttl command"); goto exit;
        };
        default_ttr=strtoul(default_ttl_s.c_str(),&ep,10);
        if(*ep != 0) {
          logger.msg(Arc::ERROR,"Wrong number in defaultttl command"); goto exit;
        };
      } else {
        default_ttr=DEFAULT_KEEP_DELETED;
      };
    }
    else if(command == "maxrerun") { /* number of retries allowed */
      std::string default_reruns_s = config_next_arg(rest);
      if(default_reruns_s.length() == 0) {
        logger.msg(Arc::ERROR,"maxrerun is empty"); goto exit;
      };
      if(rest.length() != 0) {
        logger.msg(Arc::ERROR,"Junk in maxrerun command"); goto exit;
      };
      char *ep;
      default_reruns=strtoul(default_reruns_s.c_str(),&ep,10);
      if(*ep != 0) {
        logger.msg(Arc::ERROR,"Wrong number in maxrerun command"); goto exit;
      };
    }
    else if(command == "diskspace") { /* maximal amount of disk space */
      std::string default_diskspace_s = config_next_arg(rest);
      if(default_diskspace_s.length() == 0) {
        logger.msg(Arc::ERROR,"diskspace is empty"); goto exit;
      };
      if(rest.length() != 0) {
        logger.msg(Arc::ERROR,"junk in diskspace command"); goto exit;
      };
      char *ep;
      default_diskspace=strtoull(default_diskspace_s.c_str(),&ep,10);
      if(*ep != 0) {
        logger.msg(Arc::ERROR,"Wrong number in diskspace command"); goto exit;
      };
    }
    else if(command == "lrms") {
      /* set default lrms type and queue
         (optionally). Applied to all following
         'control' commands. MUST BE thing */
      default_lrms = config_next_arg(rest);
      default_queue = config_next_arg(rest);
      if(default_lrms.empty()) {
        logger.msg(Arc::ERROR,"defaultlrms is empty"); goto exit;
      };
      if(!rest.empty()) {
        logger.msg(Arc::ERROR,"Junk in defaultlrms command"); goto exit;
      };
      check_lrms_backends(default_lrms,users.Env());
    }
    else if(command == "authplugin") { /* set plugin to be called on
                                          state changes */
      std::string state_name = config_next_arg(rest);
      if(state_name.length() == 0) {
        logger.msg(Arc::ERROR,"State name for plugin is missing"); goto exit;
      };
      std::string options_s = config_next_arg(rest);
      if(options_s.length() == 0) {
        logger.msg(Arc::ERROR,"Options for plugin are missing"); goto exit;
      };
      if(!plugins.add(state_name.c_str(),options_s.c_str(),rest.c_str())) {
        logger.msg(Arc::ERROR,"Failed to register plugin for state %s",state_name);
        goto exit;
      };
    }
    else if(command == "localcred") {
      std::string timeout_s = config_next_arg(rest);
      if(timeout_s.length() == 0) {
        logger.msg(Arc::ERROR,"Timeout for plugin is missing"); goto exit;
      };
      char *ep;
      int to = strtoul(timeout_s.c_str(),&ep,10);
      if((*ep != 0) || (to<0)) {
        logger.msg(Arc::ERROR,"Wrong number for timeout in plugin command");
        goto exit;
      };
      cred_plugin = rest;
      cred_plugin.timeout(to);
    }
    else if(command == "preferredpattern") {
      std::string preferred_pattern = config_next_arg(rest);
      if(preferred_pattern.length() == 0) {
        logger.msg(Arc::ERROR, "preferredpattern value is missing");
      };
      jcfg.SetPreferredPattern(preferred_pattern);
    }
    else if(command == "sessiondir") {
      /* set session root directory - applied
         to all following 'control' commands */
      std::string session_root = config_next_arg(rest);
      if(session_root.length() == 0) {
        logger.msg(Arc::ERROR,"Session root directory is missing"); goto exit;
      };
      if(rest.length() != 0 && rest != "drain") {
        logger.msg(Arc::ERROR,"Junk in session root command"); goto exit;
      };
      session_roots.push_back(session_root);
    } else if(command == "controldir") {
      central_control_dir=rest;
    } else if(command == "control") {
      std::string control_dir = config_next_arg(rest);
      if(control_dir.length() == 0) {
        logger.msg(Arc::ERROR,"Missing directory in control command"); goto exit;
      };
      if(control_dir == "*") control_dir="";
      if(command == "controldir") rest=".";
      for(;;) {
        std::string username = config_next_arg(rest);
        if(username.length() == 0) break;
        if(username == "*") {  /* add all gridmap users */
          logger.msg(Arc::ERROR,"Gridmap user list feature is not supported anymore. Plase use @filename to specify user list."); goto exit;
        };
        if(username[0] == '@') {  /* add users from file */
          std::string filename = username.substr(1);
          if(!file_user_list(filename,rest)) {
            logger.msg(Arc::ERROR,"Can't read user list in specified file %s",filename); goto exit;
          };
          continue;
        };
        if(username == ".") {  /* accept all users in this control directory */
           /* !!!!!!! substitutions involving user names won't work !!!!!!!  */
           if(superuser) { username=""; }
           else { username=my_username; };
        };
        /* add new user to list */
        if(superuser || (my_username == username)) {
          if(users.HasUser(username)) { /* first is best */
            continue;
          };
          JobUsers::iterator user=users.AddUser(username,&cred_plugin,
                                       control_dir,&session_roots);
          if(user == users.end()) { /* bad bad user */
            logger.msg(Arc::WARNING,"Warning: creation of user \"%s\" failed",username);
          }
          else {
            std::string control_dir_ = control_dir;
            for(std::vector<std::string>::iterator i = session_roots.begin(); i != session_roots.end(); i++) {
              user->substitute(*i);
            }
            user->SetLRMS(default_lrms,default_queue);
            user->SetKeepFinished(default_ttl);
            user->SetKeepDeleted(default_ttr);
            user->SetReruns(default_reruns);
            user->SetDiskSpace(default_diskspace);
            user->substitute(control_dir_);
            user->SetControlDir(control_dir_);
            user->SetSessionRoot(session_roots);
            user->SetStrictSession(strict_session);
            // get cache parameters for this user
            try {
              CacheConfig cache_config = CacheConfig(users.Env(),user->UnixName());
              user->SetCacheParams(cache_config);
            }
            catch (CacheConfigException e) {
              logger.msg(Arc::ERROR, "Error with cache configuration: %s", e.what());
              goto exit;
            }
            /* add helper to poll for finished jobs */
            std::string cmd_ = users.Env().nordugrid_libexec_loc();
            make_escaped_string(control_dir_);
            cmd_+="/scan-"+default_lrms+"-job";
            make_escaped_string(cmd_);
            cmd_+=" --config ";
            cmd_+=users.Env().nordugrid_config_loc();
            cmd_+=" ";
            cmd_+=user->ControlDir();
            user->add_helper(cmd_);
            /* creating empty list of jobs */
            JobsList *jobs = new JobsList(*user,plugins);
            (*user)=jobs; /* back-associate jobs with user :) */
          };
        };
      };
      last_control_dir = control_dir;
      session_roots.clear();
    }
    else if(command == "helper") {
      std::string helper_user = config_next_arg(rest);
      if(helper_user.length() == 0) {
        logger.msg(Arc::ERROR,"User for helper program is missing"); goto exit;
      };
      if(rest.length() == 0) {
        logger.msg(Arc::ERROR,"Helper program is missing"); goto exit;
      };
      if(helper_user == "*") {  /* go through all configured users */
        for(JobUsers::iterator user=users.begin();user!=users.end();++user) {
          if(!(user->has_helpers())) {
            std::string rest_=rest;
            user->substitute(rest_);
            user->add_helper(rest_);
          };
        };
      }
      else if(helper_user == ".") { /* special helper */
        std::string control_dir_ = last_control_dir;
        my_user.SetLRMS(default_lrms,default_queue);
        my_user.SetKeepFinished(default_ttl);
        my_user.SetKeepDeleted(default_ttr);
        my_user.substitute(control_dir_);
        my_user.SetSessionRoot(session_roots);
        my_user.SetControlDir(control_dir_);
        std::string my_helper=rest;
        users.substitute(my_helper);
        my_user.substitute(my_helper);
        my_user.add_helper(my_helper);
      }
      else {
        /* look for that user */
        JobUsers::iterator user=users.find(helper_user);
        if(user == users.end()) {
          logger.msg(Arc::ERROR,"%s user for helper program is not configured",helper_user);
          goto exit;
        };
        user->substitute(rest);
        user->add_helper(rest);
      };
    };
  };
  delete cf;
  config_close(cfile);
  if(infosys_user.length()) {
    struct passwd pw_;
    struct passwd *pw;
    char buf[BUFSIZ];
    getpwnam_r(infosys_user.c_str(),&pw_,buf,BUFSIZ,&pw);
    if(pw != NULL) {
      if(pw->pw_uid != 0) {
        for(JobUsers::iterator user=users.begin();user!=users.end();++user) {
          user->SetShareID(pw->pw_uid);
        };
      };
    };
  };
  /*
  if(daemon) {
    if(jobreport_key.empty()) jobreport_key = daemon->keypath();
    if(jobreport_cert.empty()) jobreport_cert = daemon->certpath();
    if(jobreport_cadir.empty()) jobreport_cadir = daemon->cadirpath();
  }
  */
  users.Env().job_log().set_credentials(jobreport_key,jobreport_cert,jobreport_cadir);
  return true;
exit:
  delete cf;
  config_close(cfile);
  return false;
}

bool configure_serviced_users(Arc::XMLNode cfg,JobUsers &users,uid_t my_uid,const std::string &my_username,JobUser &my_user) {
  Arc::XMLNode tmp_node;
  bool superuser = (my_uid == 0);
  std::string default_lrms;
  std::string default_queue;
  std::string last_control_dir;
  std::vector<std::string> session_roots;
  JobsListConfig& jcfg = users.Env().jobs_cfg();
  /*
   Currently we have everything running inside same arched.
   So we do not need any special treatment for infosys.
    std::string infosys_user("");
  */
  /*
  jobLogPath

  jobReport
    destination
    expiration
    type
    parameters
    keyPath
    certificatePath
    CACertificatesDir
  */
  tmp_node = cfg["jobLogPath"];
  if(tmp_node) {
    std::string fname = tmp_node;
    users.Env().job_log().SetOutput(fname.c_str());
  };
  tmp_node = cfg["jobReport"];
  if(tmp_node) {
    std::string url = tmp_node["destination"];
    if(!url.empty()) {
      // destination is required
      users.Env().job_log().SetReporter(url.c_str());
      unsigned int i;
      if(Arc::stringto(tmp_node["expiration"],i)) users.Env().job_log().SetExpiration(i);
      std::string parameters = tmp_node["parameters"];
      if(!parameters.empty()) users.Env().job_log().set_options(parameters);
      std::string jobreport_key = tmp_node["keyPath"];
      std::string jobreport_cert = tmp_node["certificatePath"];
      std::string jobreport_cadir = tmp_node["CACertificatesDir"];
      users.Env().job_log().set_credentials(jobreport_key,jobreport_cert,jobreport_cadir);
    };
  };

  /*
  loadLimits
    maxJobsTracked
    maxJobsRun
    maxJobsTransferred
    maxJobsTransferredAdditional
    maxFilesTransferred
    maxLoadShare
    loadShareType
    wakeupPeriod
  */
  tmp_node = cfg["loadLimits"];
  if(tmp_node) {
    int max_jobs = -1;
    int max_jobs_running = -1;
    int max_jobs_processing = -1;
    int max_jobs_processing_emergency = -1;
    int max_downloads = -1;
    int max_jobs_per_dn = -1;
    int max_share;
    unsigned int wakeup_period = jcfg.WakeupPeriod();
    elementtoint(tmp_node,"maxJobsTracked",max_jobs,&logger);
    elementtoint(tmp_node,"maxJobsRun",max_jobs_running,&logger);
    elementtoint(tmp_node,"maxJobsPerDN",max_jobs_per_dn,&logger);
    jcfg.SetMaxJobs(max_jobs,max_jobs_running,max_jobs_per_dn);
    elementtoint(tmp_node,"maxJobsTransferred",max_jobs_processing,&logger);
    // Included for backward compatibility.
    if (!tmp_node["maxJobsTransferred"] && tmp_node["maxJobsTransfered"]) {
      elementtoint(tmp_node,"maxJobsTransfered",max_jobs_processing,&logger);
    }
    elementtoint(tmp_node,"maxJobsTransferredAdditional",max_jobs_processing_emergency,&logger);
    // Included for backward compatibility.
    if (!tmp_node["maxJobsTransferredAdditional"] && tmp_node["maxJobsTransferedAdditional"]) {
      elementtoint(tmp_node,"maxJobsTransferedAdditional",max_jobs_processing_emergency,&logger);
    }
    elementtoint(tmp_node,"maxFilesTransferred",max_downloads,&logger);
    // Included for backward compatibility.
    if (!tmp_node["maxFilesTransferred"] && tmp_node["maxFilesTransfered"]) {
      elementtoint(tmp_node,"maxFilesTransfered",max_downloads,&logger);
    }
    jcfg.SetMaxJobsLoad(max_jobs_processing,
                             max_jobs_processing_emergency,
                             max_downloads);
    std::string transfer_share = tmp_node["loadShareType"];
    if(elementtoint(tmp_node,"maxLoadShare",max_share,&logger) && (max_share > 0) && ! transfer_share.empty()){
  jcfg.SetTransferShare(max_share, transfer_share);
    }
    if(elementtoint(tmp_node,"wakeupPeriod",wakeup_period,&logger)) {
      jcfg.SetWakeupPeriod(wakeup_period);
    };
    Arc::XMLNode share_limit_node;
    share_limit_node = tmp_node["shareLimit"];
    for(;share_limit_node;++share_limit_node) {
      int share_limit = -1;
      std::string limited_share = share_limit_node["name"];
      if(elementtoint(share_limit_node,"limit",share_limit,&logger) && (share_limit > 0) && ! limited_share.empty()) {
        jcfg.AddLimitedShare(limited_share,share_limit);
      }
    }
  }

  /*
  dataTransfer
    secureTransfer
    passiveTransfer
    localTransfer
    preferredPattern
    timeouts
      minSpeed
      minSpeedTime
      minAverageSpeed
      maxInactivityTime
    maxRetries
    mapURL (link)
      from
      to
    Globus
      gridmapfile
      cadir
      certpath
      keypath
      TCPPortRange
      UDPPortRange
    httpProxy
  */
  tmp_node = cfg["dataTransfer"];
  if(tmp_node) {
    int min_speed=0;
    int min_speed_time=300;
    int min_average_speed=0;
    int max_inactivity_time=300;
    int max_retries = DEFAULT_MAX_RETRIES;
    bool use_secure_transfer = false;
    bool use_passive_transfer = true;
    bool use_local_transfer = false;
    Arc::XMLNode to_node = tmp_node["timeouts"];
    if(to_node) {
      elementtoint(tmp_node,"minSpeed",min_speed,&logger);
      elementtoint(tmp_node,"minSpeedTime",min_speed_time,&logger);
      elementtoint(tmp_node,"minAverageSpeed",min_average_speed,&logger);
      elementtoint(tmp_node,"maxInactivityTime",max_inactivity_time,&logger);
      jcfg.SetSpeedControl(min_speed,min_speed_time,
                                min_average_speed,max_inactivity_time);
    };
    elementtobool(tmp_node,"passiveTransfer",use_passive_transfer,&logger);
    jcfg.SetPassiveTransfer(use_passive_transfer);
    elementtobool(tmp_node,"secureTransfer",use_secure_transfer,&logger);
    jcfg.SetSecureTransfer(use_secure_transfer);
    elementtobool(tmp_node,"localTransfer",use_local_transfer,&logger);
    jcfg.SetLocalTransfer(use_local_transfer);
    if(elementtoint(tmp_node,"maxRetries",max_retries,&logger) && (max_retries > 0)) {
        jcfg.SetMaxRetries(max_retries);
    }
    std::string preferred_pattern = tmp_node["preferredPattern"];
    jcfg.SetPreferredPattern(preferred_pattern);
  };
  /*
  serviceMail
  */
  tmp_node = cfg["serviceMail"];
  if(tmp_node) {
    users.Env().support_mail_address((std::string)tmp_node);
    if(users.Env().support_mail_address().empty()) {
      logger.msg(Arc::ERROR,"serviceMail is empty");
      return false;
    };
  }

  /*
  LRMS
    type
    defaultShare
  */
  tmp_node = cfg["LRMS"];
  if(tmp_node) {
    default_lrms = (std::string)(tmp_node["type"]);
    if(default_lrms.empty()) {
      logger.msg(Arc::ERROR,"Type in LRMS is missing"); return false;
    };
    default_queue = (std::string)(tmp_node["defaultShare"]);
    check_lrms_backends(default_lrms,users.Env());
    users.Env().runtime_config_dir((std::string)(tmp_node["runtimeDir"]));
  } else {
    logger.msg(Arc::ERROR,"LRMS is missing"); return false;
  }

  /*
  authPlugin (timeout,onSuccess=PASS,FAIL,LOG,onFailure=FAIL,PASS,LOG,onTimeout=FAIL,PASS,LOG)
    state
    command
  */
  tmp_node = cfg["authPlugin"];
  for(;tmp_node;++tmp_node) {
    std::string state_name = tmp_node["state"];
    if(state_name.empty()) {
      logger.msg(Arc::ERROR,"State name for authPlugin is missing");
      return false;
    };
    std::string command = tmp_node["command"];
    if(state_name.empty()) {
      logger.msg(Arc::ERROR,"Command for authPlugin is missing");
      return false;
    };
    std::string options;
    Arc::XMLNode onode;
    onode = tmp_node.Attribute("timeout");
    if(onode) options+="timeout="+(std::string)onode+',';
    onode = tmp_node.Attribute("onSuccess");
    if(onode) options+="onsuccess="+Arc::lower((std::string)onode)+',';
    onode = tmp_node.Attribute("onFailure");
    if(onode) options+="onfailure="+Arc::lower((std::string)onode)+',';
    onode = tmp_node.Attribute("onTimeout");
    if(onode) options+="ontimeout="+Arc::lower((std::string)onode)+',';
    if(!options.empty()) options=options.substr(0,options.length()-1);
    logger.msg(Arc::DEBUG,"Registering plugin for state %s; options: %s; commnad: %s",
        state_name.c_str(),options.c_str(),command.c_str());
    if(!plugins.add(state_name.c_str(),options.c_str(),command.c_str())) {
      logger.msg(Arc::ERROR,"Failed to register plugin for state %s",state_name);
      return false;
    };
  };

  /*
  localCred (timeout)
    command
  */
  tmp_node = cfg["localCred"];
  if(tmp_node) {
    std::string command = tmp_node["command"];
    if(command.empty()) {
      logger.msg(Arc::ERROR,"Command for localCred is missing");
      return false;
    };
    std::string options;
    Arc::XMLNode onode;
    onode = tmp_node.Attribute("timeout");
    if(!onode) {
      logger.msg(Arc::ERROR,"Timeout for localCred is missing");
      return false;
    };
    int to;
    if(!elementtoint(onode,NULL,to,&logger)) {
      logger.msg(Arc::ERROR,"Timeout for localCred is incorrect number");
      return false;
    };
    cred_plugin = command;
    cred_plugin.timeout(to);
  }

  /*
  control
    username
    controlDir
    sessionRootDir
    cache
      location
        path
        link
      highWatermark
      lowWatermark
    defaultTTL
    defaultTTR
    maxReruns
    noRootPower
    diskSpace
  */

  tmp_node = cfg["control"];
  if(!tmp_node) {
    logger.msg(Arc::ERROR,"At least one control element must be present");
    return false;
  };
  for(;tmp_node;++tmp_node) {
    std::string control_dir = tmp_node["controlDir"];
    if(control_dir.empty()) {
      logger.msg(Arc::ERROR,"controlDir is missing"); return false;
    };
    if(control_dir == "*") control_dir="";
    session_roots.clear();
    Arc::XMLNode session_node = tmp_node["sessionRootDir"];
    std::string session_root;
    for (;session_node; ++session_node) {
      session_root = std::string(session_node);
      if(session_root.empty()) {
        logger.msg(Arc::ERROR,"sessionRootDir is missing"); return false;
      };
      if (session_root.find(' ') != std::string::npos)
        session_root = session_root.substr(0, session_root.find(' '));
      session_roots.push_back(session_root);
    }
    last_control_dir = control_dir;
    bool strict_session = false;
    if(!elementtobool(tmp_node,"noRootPower",strict_session,&logger)) return false;
    unsigned int default_reruns = DEFAULT_JOB_RERUNS;
    unsigned int default_ttl = DEFAULT_KEEP_FINISHED;
    unsigned int default_ttr = DEFAULT_KEEP_DELETED;
    int default_diskspace = DEFAULT_DISKSPACE;
    if(!elementtoint(tmp_node,"maxReruns",default_reruns,&logger)) return false;
    if(!elementtoint(tmp_node,"defaultTTL",default_ttl,&logger)) return false;
    if(!elementtoint(tmp_node,"defaultTTR",default_ttr,&logger)) return false;
    if(!elementtoint(tmp_node,"defaultDiskSpace",default_diskspace,&logger)) return false;
    Arc::XMLNode unode = tmp_node["username"];
    std::list<std::string> userlist;
    for(;unode;++unode) {
      std::string username = unode;
      if(username.empty()) {
        logger.msg(Arc::ERROR,"Username in control is empty"); return false;
      };
      if(username == "*") {  /* add all gridmap users */
        logger.msg(Arc::ERROR,"Gridmap user list feature is not supported anymore. Please use @filename to specify user list.");
        return false;
      };
      if(username[0] == '@') {  /* add users from file */
        std::string filename = username.substr(1);
        if(!file_user_list(filename,userlist)) {
          logger.msg(Arc::ERROR,"Can't read users in specified file %s",filename);
          return false;
        };
        continue;
      };
      if(username == ".") {  /* accept all users in this control directory */
         /* !!!!!!! substitutions involving user names won't work !!!!!!!  */
         if(superuser) { username=""; }
         else { username=my_username; };
      };
      userlist.push_back(username);
    };
    if(userlist.size() == 0) {
      logger.msg(Arc::ERROR,"No username entries in control directory"); return false;
    };
    for(std::list<std::string>::iterator username = userlist.begin();
                   username != userlist.end();++username) {
      /* add new user to list */
      if(superuser || (my_username == *username)) {
        if(users.HasUser(*username)) { /* first is best */
          continue;
        };
        JobUsers::iterator user=users.AddUser(*username,&cred_plugin,
                                     control_dir,&session_roots);
        if(user == users.end()) { /* bad bad user */
          logger.msg(Arc::WARNING,"Warning: creation of user \"%s\" failed",*username);
        }
        else {
          std::string control_dir_ = control_dir;
          user->SetLRMS(default_lrms,default_queue);
          user->SetKeepFinished(default_ttl);
          user->SetKeepDeleted(default_ttr);
          user->SetReruns(default_reruns);
          user->SetDiskSpace(default_diskspace);
          user->substitute(control_dir_);
          for(std::vector<std::string>::iterator i = session_roots.begin(); i != session_roots.end(); i++) {
            user->substitute(*i);
          }
          user->SetControlDir(control_dir_);
          user->SetSessionRoot(session_roots);
          user->SetStrictSession(strict_session);
          // get cache parameters for this user
          try {
            CacheConfig cache_config(users.Env(),user->UnixName());
            user->SetCacheParams(cache_config);
          }
          catch (CacheConfigException e) {
            logger.msg(Arc::ERROR, "Error with cache configuration: %s", e.what());
            return false;
          }
          /* add helper to poll for finished jobs */
          std::string cmd_ = users.Env().nordugrid_libexec_loc();
          make_escaped_string(control_dir_);
          cmd_+="/scan-"+default_lrms+"-job";
          make_escaped_string(cmd_);
          cmd_+=" --config ";
          cmd_+=users.Env().nordugrid_config_loc();
          cmd_+=" ";
          cmd_+=user->ControlDir();
          user->add_helper(cmd_);
          /* creating empty list of jobs */
          JobsList *jobs = new JobsList(*user,plugins);
          (*user)=jobs; /* back-associate jobs with user :) */
        };
      };
    };
  };
  /*
  helperUtility
    username
    command
  */
  tmp_node = cfg["helperUtility"];
  for(;tmp_node;++tmp_node) {
    std::string command = tmp_node["command"];
    if(command.empty()) {
      logger.msg(Arc::ERROR,"Command in helperUtility is missing");
      return false;
    };
    Arc::XMLNode unode = tmp_node["username"];
    for(;unode;++unode) {
      std::string username = unode;
      if(username.empty()) {
        logger.msg(Arc::ERROR,"Username in helperUtility is empty");
        return false;
      };
      if(username == "*") {  /* go through all configured users */
        for(JobUsers::iterator user=users.begin();user!=users.end();++user) {
          if(!(user->has_helpers())) {
            std::string command_=command;
            user->substitute(command_);
            user->add_helper(command_);
          };
        };
      }
      else if(username == ".") { /* special helper */
        // Take parameters of last control
        std::string control_dir_ = last_control_dir;
        my_user.SetLRMS(default_lrms,default_queue);
        my_user.substitute(control_dir_);
        my_user.SetSessionRoot(session_roots);
        my_user.SetControlDir(control_dir_);
        std::string command_=command;
        users.substitute(command_);
        my_user.substitute(command_);
        my_user.add_helper(command_);
      }
      else {
        /* look for that user */
        JobUsers::iterator user=users.find(username);
        if(user == users.end()) {
          logger.msg(Arc::ERROR,"User %s for helperUtility is not configured",
                     username);
          return false;
        };
        std::string command_=command;
        user->substitute(command_);
        user->add_helper(command_);
      };
    };
  };
  return true;
}

bool print_serviced_users(const JobUsers &users) {
  for(JobUsers::const_iterator user = users.begin();user!=users.end();++user) {
    logger.msg(Arc::INFO,"Added user : %s",user->UnixName());
    for(std::vector<std::string>::const_iterator i = user->SessionRoots().begin(); i != user->SessionRoots().end(); i++)
      logger.msg(Arc::INFO,"\tSession root dir : %s",*i);
    logger.msg(Arc::INFO,"\tControl dir      : %s",user->ControlDir());
    logger.msg(Arc::INFO,"\tdefault LRMS     : %s",user->DefaultLRMS());
    logger.msg(Arc::INFO,"\tdefault queue    : %s",user->DefaultQueue());
    logger.msg(Arc::INFO,"\tdefault ttl      : %u",user->KeepFinished());

    CacheConfig cache_config = user->CacheParams();

    std::vector<std::string> conf_caches = cache_config.getCacheDirs();
    std::vector<std::string> remote_conf_caches = cache_config.getRemoteCacheDirs();
    if(conf_caches.empty()) {
      logger.msg(Arc::INFO,"No valid caches found in configuration, caching is disabled");
      continue;
    }
    // list each cache
    for (std::vector<std::string>::iterator i = conf_caches.begin(); i != conf_caches.end(); i++) {
      logger.msg(Arc::INFO, "\tCache            : %s", (*i).substr(0, (*i).find(" ")));
      if ((*i).find(" ") != std::string::npos)
        logger.msg(Arc::INFO, "\tCache link dir   : %s", (*i).substr((*i).find_last_of(" ")+1, (*i).length()-(*i).find_last_of(" ")+1));
    }
    // list each remote cache
    for (std::vector<std::string>::iterator i = remote_conf_caches.begin(); i != remote_conf_caches.end(); i++) {
      logger.msg(Arc::INFO, "\tRemote cache     : %s", (*i).substr(0, (*i).find(" ")));
      if ((*i).find(" ") != std::string::npos)
        logger.msg(Arc::INFO, "\tRemote cache link: %s", (*i).substr((*i).find_last_of(" ")+1, (*i).length()-(*i).find_last_of(" ")+1));
    }
    if (cache_config.cleanCache())
      logger.msg(Arc::INFO, "\tCache cleaning enabled");
    else
      logger.msg(Arc::INFO, "\tCache cleaning disabled");
  };
  return true;
}

