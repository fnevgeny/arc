#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// perftest.cpp

#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <glibmm/thread.h>
#include <glibmm/timer.h>

#include <arc/GUID.h>
#include <arc/ArcConfig.h>
#include <arc/UserConfig.h>
#include <arc/Logger.h>
#include <arc/URL.h>
#include <arc/message/PayloadSOAP.h>
#include <arc/message/MCC.h>
#include <arc/communication/ClientInterface.h>

// Some global shared variables...
Glib::Mutex* mutex;
bool run;
int finishedThreads;
unsigned long completedRequests;
unsigned long failedRequests;
unsigned long totalRequests;
Glib::TimeVal completedTime;
Glib::TimeVal failedTime;
Glib::TimeVal totalTime;
std::string url_str;
bool alwaysReconnect = false;

// Round off a double to an integer.
int Round(double x){
  return int(x+0.5);
}

// Send requests and collect statistics.
void sendRequests(){
  // Some variables...
  unsigned long completedRequests = 0;
  unsigned long failedRequests = 0;
  Glib::TimeVal completedTime(0,0);
  Glib::TimeVal failedTime(0,0);
  Glib::TimeVal tBefore;
  Glib::TimeVal tAfter;
  bool connected;

  //std::string url_str("https://127.0.0.1:60000/echo");
  Arc::URL url(url_str);

  Arc::MCCConfig mcc_cfg;
  Arc::UserConfig usercfg("");
  usercfg.ApplyToConfig(mcc_cfg);
  
  Arc::NS echo_ns; echo_ns["echo"]="http://www.nordugrid.org/schemas/echo";
  
  while(run){
    
    // Create a Client.
    Arc::ClientSOAP *client = NULL;
    client = new Arc::ClientSOAP(mcc_cfg,url,60);

    connected=true;
    while(run and connected){
      // Prepare the request.
      Arc::PayloadSOAP req(echo_ns);
      req.NewChild("echo:echo").NewChild("echo:say")="HELLO";

      // Send the request and time it.
      tBefore.assign_current_time();
      Arc::PayloadSOAP* resp = NULL;

      //std::string str;
      //req.GetXML(str);
      //std::cout<<"request: "<<str<<std::endl;
      Arc::MCC_Status status = client->process(&req,&resp);

      tAfter.assign_current_time();
      
      if(!status) {
        // Request failed.
        failedRequests++;
        failedTime+=tAfter-tBefore;
	    connected=false;
      } else {
        if(resp == NULL) {
          // Response was not SOAP or no response at all.
          failedRequests++;
          failedTime+=tAfter-tBefore;
          connected=false;
        } else {
          //std::string xml;
          //resp->GetXML(xml);
          if (std::string((*resp)["echoResponse"]["hear"]).size()==0){
            // The response was not what it should be.
            failedRequests++;
            failedTime+=tAfter-tBefore;
            connected=false;
          }
          else{
            // Everything worked just fine!
            completedRequests++;
            completedTime+=tAfter-tBefore;
          }
        }
      }
      if(resp) delete resp;
      if(alwaysReconnect) connected=false;
    }
    if(client) delete client;
  }

  // Update global variables.
  Glib::Mutex::Lock lock(*mutex);
  ::completedRequests+=completedRequests;
  ::failedRequests+=failedRequests;
  ::completedTime+=completedTime;
  ::failedTime+=failedTime;
  finishedThreads++;
  std::cout << "Number of finished threads: " << finishedThreads << std::endl;
}

int main(int argc, char* argv[]){
  // Some variables...
  int numberOfThreads;
  int duration;
  int i;
  Glib::Thread** threads;
  const char* config_file = NULL;
  int debug_level = -1;
  Arc::LogStream logcerr(std::cerr);

  // Process options - quick hack, must use Glib options later
  while(argc >= 3) {
    if(strcmp(argv[1],"-c") == 0) {
      config_file = argv[2];
      argv[2]=argv[0]; argv+=2; argc-=2;
    } else if(strcmp(argv[1],"-d") == 0) {
      debug_level=Arc::istring_to_level(argv[2]);
      argv[2]=argv[0]; argv+=2; argc-=2;
    } else if(strcmp(argv[1],"-r") == 0) {
      alwaysReconnect=true; argv+=1; argc-=1;
    } else {
      break;
    };
  } 
  if(debug_level >= 0) {
    Arc::Logger::getRootLogger().setThreshold((Arc::LogLevel)debug_level);
    Arc::Logger::getRootLogger().addDestination(logcerr);
  }
  // Extract command line arguments.
  if (argc!=4){
    std::cerr << "Wrong number of arguments!" << std::endl
	      << std::endl
	      << "Usage:" << std::endl
	      << "perftest [-c config] [-d debug] [-r] url threads duration" << std::endl
	      << std::endl
	      << "Arguments:" << std::endl
	      << "url       The url of the service." << std::endl
	      << "threads   The number of concurrent requests." << std::endl
	      << "duration  The duration of the test in seconds." << std::endl
	      << "-c config The file containing client chain XML configuration with " << std::endl
              << "          'soap' entry point and HOSTNAME, PORTNUMBER and PATH " << std::endl
              << "           keyword for hostname, port and HTTP path of 'echo' service." << std::endl
	      << "-d debug   The textual representation of desired debug level. Available " << std::endl
              << "           levels: DEBUG, VERBOSE, INFO, WARNING, ERROR, FATAL." << std::endl
	      << "-r         If specified close connection and reconnect after " << std::endl
              << "           every request." << std::endl;
    exit(EXIT_FAILURE);
  }
  url_str = std::string(argv[1]);
  numberOfThreads = atoi(argv[2]);
  duration = atoi(argv[3]);

  // Start threads.
  run=true;
  finishedThreads=0;
  //Glib::thread_init();
  mutex=new Glib::Mutex;
  threads = new Glib::Thread*[numberOfThreads];
  for (i=0; i<numberOfThreads; i++)
    threads[i]=Glib::Thread::create(sigc::ptr_fun(sendRequests),true);

  // Sleep while the threads are working.
  Glib::usleep(duration*1000000);

  // Stop the threads
  run=false;
  while(finishedThreads<numberOfThreads)
    Glib::usleep(100000);

  // Print the result of the test.
  Glib::Mutex::Lock lock(*mutex);
  totalRequests = completedRequests+failedRequests;
  totalTime = completedTime+failedTime;
  std::cout << "========================================" << std::endl;
  std::cout << "URL: "
	    << url_str << std::endl;
  std::cout << "Number of threads: "
	    << numberOfThreads << std::endl;
  std::cout << "Duration: "
	    << duration << " s" << std::endl;
  std::cout << "Number of requests: "
	    << totalRequests << std::endl;
  std::cout << "Completed requests: "
	    << completedRequests << " ("
	    << Round(completedRequests*100.0/totalRequests)
	    << "%)" << std::endl;
  std::cout << "Failed requests: "
	    << failedRequests << " ("
	    << Round(failedRequests*100.0/totalRequests)
	    << "%)" << std::endl;
  std::cout << "Completed requests per second: "
            << Round(completedRequests/duration)
            << std::endl;
  std::cout << "Average response time for all requests: "
	    << Round(1000*totalTime.as_double()/totalRequests)
	    << " ms" << std::endl;
  if (completedRequests!=0)
    std::cout << "Average response time for completed requests: "
	      << Round(1000*completedTime.as_double()/completedRequests)
	      << " ms" << std::endl;
  if (failedRequests!=0)
    std::cout << "Average response time for failed requests: "
	      << Round(1000*failedTime.as_double()/failedRequests)
	      << " ms" << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
