// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_EXECUTIONTARGET_H__
#define __ARC_EXECUTIONTARGET_H__

#include <list>
#include <map>
#include <set>
#include <string>

#include <arc/DateTime.h>
#include <arc/client/Endpoint.h>
#include <arc/URL.h>
#include <arc/Utils.h>
#include <arc/client/GLUE2Entity.h>
#include <arc/client/JobDescription.h>
#include <arc/client/Software.h>
#include <arc/client/Submitter.h>

namespace Arc {

  class Job;
  class Logger;
  class Submitter;
  class UserConfig;

  /// ApplicationEnvironment
  /**
   * The ApplicationEnviroment is closely related to the definition given in
   * GLUE2. By extending the Software class the two GLUE2 attributes AppName and
   * AppVersion are mapped to two private members. However these can be obtained
   * through the inheriated member methods getName and getVersion.
   *
   * GLUE2 description:
   * A description of installed application software or software environment
   * characteristics available within one or more Execution Environments.
   */
  class ApplicationEnvironment
    : public Software {
  public:
    ApplicationEnvironment() {}
    ApplicationEnvironment(const std::string& Name)
      : Software(Name) {}
    ApplicationEnvironment(const std::string& Name, const std::string& Version)
      : Software(Name, Version) {}
    ApplicationEnvironment& operator=(const Software& sv) {
      Software::operator=(sv);
      return *this;
    }

    std::string State;
    int FreeSlots;
    int FreeJobs;
    int FreeUserSeats;
  };

  class LocationAttributes {
  public:
    LocationAttributes() : Latitude(0), Longitude(0) {}

    std::string Address;
    std::string Place;
    std::string Country;
    std::string PostCode;
    float Latitude;
    float Longitude;
  };

  class AdminDomainAttributes {
  public:
    std::string Name;
    std::string Owner;
  };

  class ExecutionEnvironmentAttributes {
  public:
    ExecutionEnvironmentAttributes()
      : VirtualMachine(false), CPUClockSpeed(-1), MainMemorySize(-1),
        ConnectivityIn(false), ConnectivityOut(false) {}
    std::string ID;
    std::string Platform;
    bool VirtualMachine;
    std::string CPUVendor;
    std::string CPUModel;
    std::string CPUVersion;
    int CPUClockSpeed;
    int MainMemorySize;

    /// OperatingSystem
    /**
     * The OperatingSystem member is not present in GLUE2 but contains the three
     * GLUE2 attributes OSFamily, OSName and OSVersion.
     * - OSFamily OSFamily_t 1
     *   * The general family to which the Execution Environment operating
     *   * system belongs.
     * - OSName OSName_t 0..1
     *   * The specific name of the operating sytem
     * - OSVersion String 0..1
     *   * The version of the operating system, as defined by the vendor.
     */
    Software OperatingSystem;

    bool ConnectivityIn;
    bool ConnectivityOut;
  };

  class ComputingManagerAttributes {
  public:
    ComputingManagerAttributes() :
      Reservation(false), BulkSubmission(false),
      TotalPhysicalCPUs(-1), TotalLogicalCPUs(-1), TotalSlots(-1),
      Homogeneous(true),
      WorkingAreaShared(true), WorkingAreaTotal(-1), WorkingAreaFree(-1), WorkingAreaLifeTime(-1),
      CacheTotal(-1), CacheFree(-1) {}

    std::string ID;
    std::string ProductName;
    std::string ProductVersion;
    bool Reservation;
    bool BulkSubmission;
    int TotalPhysicalCPUs;
    int TotalLogicalCPUs;
    int TotalSlots;
    bool Homogeneous;
    std::list<std::string> NetworkInfo;
    bool WorkingAreaShared;
    int WorkingAreaTotal;
    int WorkingAreaFree;
    Period WorkingAreaLifeTime;
    int CacheTotal;
    int CacheFree;
  };

  class ComputingShareAttributes {
  public:
    ComputingShareAttributes() :
      MaxWallTime(-1), MaxTotalWallTime(-1), MinWallTime(-1), DefaultWallTime(-1),
      MaxCPUTime(-1), MaxTotalCPUTime(-1), MinCPUTime(-1), DefaultCPUTime(-1),
      MaxTotalJobs(-1), MaxRunningJobs(-1), MaxWaitingJobs(-1),
      MaxPreLRMSWaitingJobs(-1), MaxUserRunningJobs(-1), MaxSlotsPerJob(-1),
      MaxStageInStreams(-1), MaxStageOutStreams(-1),
      MaxMainMemory(-1), MaxVirtualMemory(-1), MaxDiskSpace(-1),
      Preemption(false),
      TotalJobs(-1), RunningJobs(-1), LocalRunningJobs(-1), WaitingJobs(-1), LocalWaitingJobs(-1),
      SuspendedJobs(-1), LocalSuspendedJobs(-1), StagingJobs(-1), PreLRMSWaitingJobs(-1),
      EstimatedAverageWaitingTime(-1), EstimatedWorstWaitingTime(-1),
      FreeSlots(-1), UsedSlots(-1), RequestedSlots(-1) {}

    std::string ID;
    /// Name String 0..1
    /**
     * Human-readable name.
     * This variable represents the ComputingShare.Name attribute of GLUE2.
     **/
    std::string Name;
    std::string MappingQueue;

    Period MaxWallTime;
    Period MaxTotalWallTime; // not in current Glue2 draft
    Period MinWallTime;
    Period DefaultWallTime;
    Period MaxCPUTime;
    Period MaxTotalCPUTime;
    Period MinCPUTime;
    Period DefaultCPUTime;
    int MaxTotalJobs;
    int MaxRunningJobs;
    int MaxWaitingJobs;
    int MaxPreLRMSWaitingJobs;
    int MaxUserRunningJobs;
    int MaxSlotsPerJob;
    int MaxStageInStreams;
    int MaxStageOutStreams;
    std::string SchedulingPolicy;

    /// MaxMainMemory UInt64 0..1 MB
    /**
     * The maximum physical RAM that a job is allowed to use; if the limit is
     * hit, then the LRMS MAY kill the job.
     * A negative value specifies that this member is undefined.
     */
    int MaxMainMemory;

    /// MaxVirtualMemory UInt64 0..1 MB
    /**
     * The maximum total memory size (RAM plus swap) that a job is allowed to
     * use; if the limit is hit, then the LRMS MAY kill the job.
     * A negative value specifies that this member is undefined.
     */
    int MaxVirtualMemory;

    /// MaxDiskSpace UInt64 0..1 GB
    /**
     * The maximum disk space that a job is allowed use in the working; if the
     * limit is hit, then the LRMS MAY kill the job.
     * A negative value specifies that this member is undefined.
     */
    int MaxDiskSpace;

    URL DefaultStorageService;
    bool Preemption;
    int TotalJobs;
    int RunningJobs;
    int LocalRunningJobs;
    int WaitingJobs;
    int LocalWaitingJobs;
    int SuspendedJobs;
    int LocalSuspendedJobs;
    int StagingJobs;
    int PreLRMSWaitingJobs;
    Period EstimatedAverageWaitingTime;
    Period EstimatedWorstWaitingTime;
    int FreeSlots;

    /// FreeSlotsWithDuration std::map<Period, int>
    /**
     * This attribute express the number of free slots with their time limits.
     * The keys in the std::map are the time limit (Period) for the number of
     * free slots stored as the value (int). If no time limit has been specified
     * for a set of free slots then the key will equal Period(LONG_MAX).
     */
    std::map<Period, int> FreeSlotsWithDuration;
    int UsedSlots;
    int RequestedSlots;
    std::string ReservationPolicy;
  };

  class ComputingEndpointAttributes {
  public:
    ComputingEndpointAttributes() : DowntimeStarts(-1), DowntimeEnds(-1),
      TotalJobs(-1), RunningJobs(-1), WaitingJobs(-1),
      StagingJobs(-1), SuspendedJobs(-1), PreLRMSWaitingJobs(-1) {}

    std::string ID;
    std::string URLString;
    std::string InterfaceName;
    std::string HealthState;
    std::string HealthStateInfo;
    std::string QualityLevel;
    std::list<std::string> Capability;
    std::string Technology;
    std::list<std::string> InterfaceVersion;
    std::list<std::string> InterfaceExtension;
    std::list<std::string> SupportedProfile;
    std::string Implementor;
    Software Implementation;
    std::string ServingState;
    std::string IssuerCA;
    std::list<std::string> TrustedCA;
    Time DowntimeStarts;
    Time DowntimeEnds;
    std::string Staging;
    int TotalJobs;
    int RunningJobs;
    int WaitingJobs;
    int StagingJobs;
    int SuspendedJobs;
    int PreLRMSWaitingJobs;
    // This is singular in the GLUE2 doc: JobDescription
    std::list<std::string> JobDescriptions;
  };

  class ComputingServiceAttributes {
  public:
    ComputingServiceAttributes() :
      TotalJobs(-1), RunningJobs(-1), WaitingJobs(-1),
      StagingJobs(-1), SuspendedJobs(-1), PreLRMSWaitingJobs(-1) {}
    std::string ID;
    std::string Name;
    std::string Type;
    std::list<std::string> Capability;
    std::string QualityLevel;
    int TotalJobs;
    int RunningJobs;
    int WaitingJobs;
    int StagingJobs;
    int SuspendedJobs;
    int PreLRMSWaitingJobs;

    // Other
    URL Cluster; // contains the URL of the infosys that provided the info
    Endpoint OriginalEndpoint; // this ComputingService was generated while this Endpoint was queried
    };

  class LocationType : public GLUE2Entity<LocationAttributes> {};

  class AdminDomainType : public GLUE2Entity<AdminDomainAttributes> {};

  class ExecutionEnvironmentType : public GLUE2Entity<ExecutionEnvironmentAttributes> {};

  class ComputingManagerType : public GLUE2Entity<ComputingManagerAttributes> {
  public:
    ComputingManagerType() : Benchmarks(new std::map<std::string, double>), ApplicationEnvironments(new std::list<ApplicationEnvironment>) {}
    // TODO: Currently using int as key, use std::string instead for holding ID.
    std::map<int, ExecutionEnvironmentType> ExecutionEnvironment;
    CountedPointer< std::map<std::string, double> > Benchmarks;
    /// ApplicationEnvironments
    /**
     * The ApplicationEnvironments member is a list of
     * ApplicationEnvironment's, defined in section 6.7 GLUE2.
     */
    CountedPointer< std::list<ApplicationEnvironment> > ApplicationEnvironments;
  };

  class ComputingShareType : public GLUE2Entity<ComputingShareAttributes> {
  public:
    // TODO: Currently using int, use std::string instead for holding ID.
    std::set<int> ComputingEndpointIDs;
  };

  class ComputingEndpointType : public GLUE2Entity<ComputingEndpointAttributes> {
  public:
    // TODO: Currently using int, use std::string instead for holding ID.
    std::set<int> ComputingShareIDs;
  };

  class ComputingServiceType : public GLUE2Entity<ComputingServiceAttributes> {
  public:
    void GetExecutionTargets(std::list<ExecutionTarget>& etList) const;

    LocationType Location;
    AdminDomainType AdminDomain;
    // TODO: Currently using int as key, use std::string instead for holding ID.
    std::map<int, ComputingEndpointType> ComputingEndpoint;
    std::map<int, ComputingShareType> ComputingShare;
    std::map<int, ComputingManagerType> ComputingManager;
  };

  /// ExecutionTarget
  /**
   * This class describe a target which accept computing jobs. All of the
   * members contained in this class, with a few exceptions, are directly
   * linked to attributes defined in the GLUE Specification v. 2.0
   * (GFD-R-P.147).
   */
  class ExecutionTarget {
  public:
    /// Create an ExecutionTarget
    /**
     * Default constructor to create an ExecutionTarget. Takes no
     * arguments.
     **/
    ExecutionTarget() :
      Location(new LocationAttributes()),
      AdminDomain(new AdminDomainAttributes()),
      ComputingService(new ComputingServiceAttributes()),
      ComputingEndpoint(new ComputingEndpointAttributes()),
      ComputingShare(new ComputingShareAttributes()),
      ComputingManager(new ComputingManagerAttributes()),
      ExecutionEnvironment(new ExecutionEnvironmentAttributes()),
      Benchmarks(new std::map<std::string, double>()),
      ApplicationEnvironments(new std::list<ApplicationEnvironment>()) {};

    /// Create an ExecutionTarget
    /**
     * Copy constructor.
     *
     * @param target ExecutionTarget to copy.
     **/
    ExecutionTarget(const ExecutionTarget& t) :
      Location(t.Location), AdminDomain(t.AdminDomain), ComputingService(t.ComputingService),
      ComputingEndpoint(t.ComputingEndpoint), ComputingShare(t.ComputingShare), ComputingManager(t.ComputingManager),
      ExecutionEnvironment(t.ExecutionEnvironment), Benchmarks(t.Benchmarks), ApplicationEnvironments(t.ApplicationEnvironments) {}

    ExecutionTarget(const CountedPointer<LocationAttributes>& l,
                    const CountedPointer<AdminDomainAttributes>& a,
                    const CountedPointer<ComputingServiceAttributes>& cse,
                    const CountedPointer<ComputingEndpointAttributes>& ce,
                    const CountedPointer<ComputingShareAttributes>& csh,
                    const CountedPointer<ComputingManagerAttributes>& cm,
                    const CountedPointer<ExecutionEnvironmentAttributes>& ee,
                    const CountedPointer< std::map<std::string, double> >& b,
                    const CountedPointer< std::list<ApplicationEnvironment> >& ae) :
      Location(l), AdminDomain(a), ComputingService(cse),
      ComputingEndpoint(ce), ComputingShare(csh), ComputingManager(cm),
      ExecutionEnvironment(ee), Benchmarks(b), ApplicationEnvironments(ae) {}

    /// Create an ExecutionTarget
    /**
     * Copy constructor? Needed from Python?
     *
     * @param addrptr
     *
     **/
    ExecutionTarget(long int addrptr) :
      Location((*(ExecutionTarget*)addrptr).Location),
      AdminDomain((*(ExecutionTarget*)addrptr).AdminDomain),
      ComputingService((*(ExecutionTarget*)addrptr).ComputingService),
      ComputingEndpoint((*(ExecutionTarget*)addrptr).ComputingEndpoint),
      ComputingShare((*(ExecutionTarget*)addrptr).ComputingShare),
      ComputingManager((*(ExecutionTarget*)addrptr).ComputingManager),
      ExecutionEnvironment((*(ExecutionTarget*)addrptr).ExecutionEnvironment),
      Benchmarks((*(ExecutionTarget*)addrptr).Benchmarks),
      ApplicationEnvironments((*(ExecutionTarget*)addrptr).ApplicationEnvironments) {}


    ExecutionTarget& operator=(const ExecutionTarget& et) {
      Location = et.Location; AdminDomain = et.AdminDomain; ComputingService = et.ComputingService;
      ComputingEndpoint = et.ComputingEndpoint; ComputingShare = et.ComputingShare; ComputingManager = et.ComputingManager;
      Benchmarks = et.Benchmarks; ExecutionEnvironment = et.ExecutionEnvironment; ApplicationEnvironments = et.ApplicationEnvironments;
      return *this;
    }

    ~ExecutionTarget() {};

    /// Get Submitter to the computing resource represented by the ExecutionTarget
    /**
     * Method which returns a specialized Submitter which can be used
     * for submitting jobs to the computing resource represented by
     * the ExecutionTarget. In order to return the correct specialized
     * Submitter the GridFlavour variable must be correctly set.
     *
     * @param ucfg UserConfig object with paths to user credentials
     * etc.
     **/
    Submitter* GetSubmitter(const UserConfig& ucfg) const;

    bool GetTestJob(const UserConfig& ucfg, const int& testid, JobDescription& jobdescription) const {
      return GetSubmitter(ucfg)->GetTestJob(testid, jobdescription);
    }

    bool Submit(const UserConfig& ucfg, const JobDescription& jobdesc, Job& job) const {
      Submitter* s = GetSubmitter(ucfg);
      if (s == NULL) {
        return false;
      }
      return s->Submit(jobdesc, job);
    }

    bool Migrate(const UserConfig& ucfg, const URL& jobid,
                 const JobDescription& jobdesc, bool forcemigration,
                 Job& job) const {
      Submitter* s = GetSubmitter(ucfg);
      if (s == NULL) {
        return false;
      }
      return s->Migrate(jobid, jobdesc, forcemigration, job);
    }


    /// Update ExecutionTarget after succesful job submission
    /**
     * Method to update the ExecutionTarget after a job succesfully
     * has been submitted to the computing resource it
     * represents. E.g. if a job is sent to the computing resource and
     * is expected to enter the queue, then the WaitingJobs attribute
     * is incremented with 1.
     *
     * @param jobdesc contains all information about the job
     * submitted.
     **/
    void Update(const JobDescription& jobdesc);

    /// Print the ExecutionTarget information to a std::ostream object
    /**
     * Method to print the ExecutionTarget attributes to a std::ostream object.
     *
     * @param out is the std::ostream to print the attributes to.
     * @param longlist should be set to true for printing a long list.
     **/
    void SaveToStream(std::ostream& out, bool longlist) const;

    static void GetExecutionTargetsOfList(const std::list<ComputingServiceType>& csList, std::list<ExecutionTarget>& etList);

    CountedPointer<LocationAttributes> Location;
    CountedPointer<AdminDomainAttributes> AdminDomain;
    CountedPointer<ComputingServiceAttributes> ComputingService;
    CountedPointer<ComputingEndpointAttributes> ComputingEndpoint;
    CountedPointer<ComputingShareAttributes> ComputingShare;
    CountedPointer<ComputingManagerAttributes> ComputingManager;
    CountedPointer<ExecutionEnvironmentAttributes> ExecutionEnvironment;
    CountedPointer< std::map<std::string, double> > Benchmarks;
    CountedPointer< std::list<ApplicationEnvironment> > ApplicationEnvironments;

  private:
    SubmitterLoader loader;

    const std::string GetPluginName() const;

    static Logger logger;
  };

} // namespace Arc

#endif // __ARC_EXECUTIONTARGET_H__
