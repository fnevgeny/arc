#ifndef __ARC_TESTACCCONTROL_H__
#define __ARC_TESTACCCONTROL_H__

#include <list>
#include <string>

#include <arc/URL.h>
#include <arc/client/Endpoint.h>
#include <arc/client/EndpointQueryingStatus.h>
#include <arc/client/ExecutionTarget.h>
#include <arc/client/Job.h>
#include <arc/client/TargetGenerator.h>
#include <arc/client/JobState.h>


namespace Arc {

class BrokerTestACCControl {
  public:
    static bool* TargetSortingDone;
    static std::list<ExecutionTarget*>* PossibleTargets;
};

class JobDescriptionParserTestACCControl {
  public:
    static bool parseStatus;
    static bool unparseStatus;
    static std::list<JobDescription> parsedJobDescriptions;
    static std::string unparsedString;
};

class JobControllerTestACCControl {
  public:
    static bool jobStatus;
    static bool cleanStatus;
    static bool cancelStatus;
    static bool renewStatus;
    static bool resumeStatus;
    static bool getJobDescriptionStatus;
    static std::string getJobDescriptionString;
    static URL fileURL;
    static URL createURL;
};

class SubmitterTestACCControl {
  public:
    static bool submitStatus;
    static bool migrateStatus;
    static bool modifyStatus;
    static Job* submitJob;
    static Job* migrateJob;
};

class TargetRetrieverTestACCControl {
  public:
    static TargetGenerator* tg;
    static std::list<ExecutionTarget> foundTargets;
    static std::list<Job> foundJobs;

    static void addTarget(ExecutionTarget target);
    static void addJob(Job job);

};

class JobStateTEST : public JobState {
  public:
    JobStateTEST(JobState::StateType type_, const std::string& state_ = "TestState") {
      type = type_;
      state = state_;
    }
};

class ServiceEndpointRetrieverPluginTESTControl {
public:
  static float delay;
  static EndpointQueryingStatus status;
  static std::list<ServiceEndpoint> endpoints;
};

class TargetInformationRetrieverPluginTESTControl {
public:
  static float delay;
  static std::list<ExecutionTarget> targets;
  static EndpointQueryingStatus status;
};

}

#endif // __ARC_TESTACCCONTROL_H__
