#include <string>

//#include "../jobs/users.h"
//#include "../files/info_files.h"
#include "../files/info_types.h"

bool parse_job_req(const std::string &fname,JobLocalDescription &job_desc,std::string* acl = NULL);
bool process_job_req(JobUser &user,const JobDescription &desc);
bool process_job_req(JobUser &user,const JobDescription &desc,JobLocalDescription &job_desc);
bool preprocess_job_req(const std::string &fname,const std::string &session_dir,const std::string &jobid);
bool write_grami(const JobDescription &desc,const JobUser &user,const char *opt_add = NULL);
std::string read_grami(const JobId &job_id,const JobUser &user);
bool set_execs(const JobDescription &desc,const JobUser &user,const std::string
&session_dir); 

