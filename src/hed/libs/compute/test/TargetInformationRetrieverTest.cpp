#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cppunit/extensions/HelperMacros.h>

#include <arc/compute/Endpoint.h>
#include <arc/UserConfig.h>
#include <arc/compute/EntityRetriever.h>
#include <arc/compute/ExecutionTarget.h>
#include <arc/compute/TestACCControl.h>
#include <arc/Thread.h>

//static Arc::Logger testLogger(Arc::Logger::getRootLogger(), "TargetInformationRetrieverTest");

class TargetInformationRetrieverTest
  : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(TargetInformationRetrieverTest);
  CPPUNIT_TEST(PluginLoading);
  CPPUNIT_TEST(QueryTest);
  CPPUNIT_TEST(GettingStatusFromUnspecifiedCE);
  CPPUNIT_TEST_SUITE_END();

public:
  TargetInformationRetrieverTest() {};

  void setUp() {}
  void tearDown() {
    Arc::ThreadInitializer().waitExit();
  }

  void PluginLoading();
  void QueryTest();
  void GettingStatusFromUnspecifiedCE();
};

void TargetInformationRetrieverTest::PluginLoading() {
  Arc::TargetInformationRetrieverPluginLoader l;
  Arc::TargetInformationRetrieverPlugin* p = (Arc::TargetInformationRetrieverPlugin*)l.load("TEST");
  CPPUNIT_ASSERT(p != NULL);
}

void TargetInformationRetrieverTest::QueryTest() {
  Arc::EndpointQueryingStatus sInitial(Arc::EndpointQueryingStatus::SUCCESSFUL);

  Arc::TargetInformationRetrieverPluginTESTControl::delay = 1;
  Arc::TargetInformationRetrieverPluginTESTControl::status = sInitial;

  Arc::TargetInformationRetrieverPluginLoader l;
  Arc::TargetInformationRetrieverPlugin* p = (Arc::TargetInformationRetrieverPlugin*)l.load("TEST");
  CPPUNIT_ASSERT(p != NULL);

  Arc::UserConfig uc;
  Arc::Endpoint endpoint;
  std::list<Arc::ComputingServiceType> csList;
  Arc::EndpointQueryingStatus sReturned = p->Query(uc, endpoint, csList, Arc::EndpointQueryOptions<Arc::ComputingServiceType>());
  CPPUNIT_ASSERT(sReturned == Arc::EndpointQueryingStatus::SUCCESSFUL);
}

void TargetInformationRetrieverTest::GettingStatusFromUnspecifiedCE() {
  // Arc::Logger logger(Arc::Logger::getRootLogger(), "TIRTest");
  // Arc::LogStream logcerr(std::cerr);
  // logcerr.setFormat(Arc::ShortFormat);
  // Arc::Logger::getRootLogger().addDestination(logcerr);
  // Arc::Logger::getRootLogger().setThreshold(Arc::DEBUG);
  
  Arc::EndpointQueryingStatus sInitial(Arc::EndpointQueryingStatus::SUCCESSFUL);

  Arc::TargetInformationRetrieverPluginTESTControl::delay = 0;
  Arc::TargetInformationRetrieverPluginTESTControl::status = sInitial;

  Arc::UserConfig uc;
  Arc::TargetInformationRetriever retriever(uc, Arc::EndpointQueryOptions<Arc::ComputingServiceType>());

  Arc::Endpoint ce("test.nordugrid.org", Arc::Endpoint::COMPUTINGINFO);
  retriever.addEndpoint(ce);
  retriever.wait();
  Arc::EndpointQueryingStatus status = retriever.getStatusOfEndpoint(ce);

  // Arc::Logger::getRootLogger().removeDestinations();

  CPPUNIT_ASSERT(status == Arc::EndpointQueryingStatus::SUCCESSFUL);
}

CPPUNIT_TEST_SUITE_REGISTRATION(TargetInformationRetrieverTest);
