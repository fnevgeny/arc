#ifndef __ARC_CREDENTIAL_H__
#define __ARC_CREDENTIAL_H__

#include <stdlib.h>
#include <stdexcept>
#include <iostream>
#include <string>
#include <openssl/asn1.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>
#include <openssl/err.h>

#include <arc/Logger.h>
#include <arc/DateTime.h>

#include "Proxycertinfo.h"
#include "cert_util.h"
  
namespace ArcLib {
  // An exception class for the Credential class.
  /** This is an exception class that is used to handle runtime errors
    discovered in the Credential class.
   */
class CredentialError : public std::runtime_error {
  public:
    // Constructor
    /** This is the constructor of the CredentialError class.
      @param what An explanation of the error.
     */
    CredentialError(const std::string& what="");
  };

typedef enum {PEM, DER, PKCS, UNKNOWN} Credformat;

class Credential {
  public:
    /**Constructor*/
    Credential();

    /**Constructor, construct from credential files, this constructor will parse the credential information,
    and put them into "this" */
    Credential(const std::string& cert, const std::string& key, const std::string& cadir, const std::string& cafile);

  private:
    /**Constructor, construct from key and request, it is just a half-matured credential*/
    Credential(X509_REQ**, EVP_PKEY** key);


    static Arc::Logger credentialLogger;

    void loadKey(BIO* &keybio, EVP_PKEY* &pkey);
    void loadCertificate(BIO* &certbio, X509* &cert, STACK_OF(X509)** certchain);
    void loadCA(BIO* &certbio, X509* &cert, STACK_OF(X509)* &certs);
    int InitProxyCertInfo(void);
    void InitVerification(void);

    /**Verify whether the certificate is signed by trusted CAs
     *the verification is not needed for EEC, but needed for verifying a proxy which 
     *is generated by the others
     */
    bool Verify(void);
 
    X509_EXTENSION* CreateExtension(std::string name, std::string data, bool crit = false);

    bool SetProxyPeriod(X509* tosign, X509* issuer, Arc::Time& start, Arc::Period& lifetime);

    bool GetCredPrivKey(EVP_PKEY** key);

    bool SignRequestAssistant(Credential* &proxy, EVP_PKEY* &req_pubkey, X509** tosign);

  /************************************/
  /*****Get the information from "this" object**/
  public:

    void LogError(void);   

    Credformat getFormat(BIO * in);        

    bool GenerateRequest(BIO* &bio);
  
    bool InquireRequest(BIO* &reqbio);

    bool SignRequest(Credential* proxy, BIO* outputbio);

  /*************************************/
  /*****General methods for proxy Request, proxy sign********/
  /**Note although we use "proxy" here, the below method can also be used for request and sign for original
  certificate*/

  public:
    /**Make a proxy request, based on the credential information inside "this"
    @param bits  modulus size for RSA key generation, it should be greater than 1024
    @param string which includes the request information, i.e, X509_REQ, which is produced by this method;
    this param is specific for transfering the request to remote end to sign.
    @return the Credential object which includes X509_REQ** and EVP_PKEY** iff successfully make the proxy request,
    otherwise NULL
    */
    //Credential* GenerateRequest(std::string& request, int bits = 1024);


    /**Make all of the extensions for proxy certificate.*
    @param name  the name of the extenstion
    @param data  data of the extension
    @return  the extension stucture iff successfully initialize extension
    */
    //X509_EXTENSION* GenerateExtension(std::string name, std::string data);


    /**Sign a certificate request, based on the proxy request which is generated by GenerateProxyRequest(),
    and certificate in "this" which is used to sign the proxy
    @param req  Credential object which includes X509_REQ* that is generated by GenerateProxyRequest(), the Credential
    object will include the signed certificate, iff successfully gets signing
    @param extensions the extension of the signed certificate
    @param lifetime  lifetime of the proxy certificate
    @param limit  whether give limit constraint for the proxy certificate
    @return  true iff successfully sign the certificate, otherwise false 
    */
    //bool Sign(Credential* req, STACK_OF(X509_EXTENSION) ** extensions, Period lifetime, bool limit);

    /**Sign a certificate request, based on the proxy request which is generated by GenerateProxyRequest(), 
    and certificate in "this" which is used to sign the proxy
    @param req  the request information
    @param extensions the extension of the signed certificate
    @param lifetime  lifetime of the proxy certificate
    @param limit  whether give limit constraint for the proxy certificate
    @return  a string which include the proxy certificate, iff successfully sign the certificate
    */
    //std::string Sign(std::string req, STACK_OF(X509_EXTENSION) ** extensions, Period lifetime, bool limit);

    /**Create a proxy certificate, based on the certificate in "this", so it will generate a self-signed proxy certificate.
    This method includes the GenerateProxyRequest, GererateProxyExtension and SignProxy.
    @param lifetime  lifetime of the proxy certificate
    @param limit  whether give limit constraint for the proxy certificate
    @return  a new Credential object
    */
    //Credential* CreateProxy(std::list<AC*> aclist, std::string extra);

    /**Set the certificate information for "this" by using proxy certificate which is return by siging side*/
    //bool SetCertificate(std::string certificate);

    /************************************/
    /*****VOMS specific methods******/
    /**AC is a struct introduced from voms code, which represents the voms attribute extracted from voms server, but
    this structure can also be use for other non-voms attribute, since voms attribute also comply with X.509 certificate
    formate*/
    //typedef struct ACC {
    //  AC_INFO         *acinfo;
    //  X509_ALGOR      *sig_alg;
    //  ASN1_BIT_STRING *signature;
    //} AC;

  public:
    /**Get the attribute information from voms server(specific method for voms credential)
    the X.509 credential attached to "this" object will be used to secure the communication with
    remote voms server.
    The aclist which is got from voms server will be used as the parameter of CreateProxy() to
    generate the proxy certificate which includes the aclist as certificate extension.
    @param voms_servers  each includes the hostname and port of voms server.
    @param aclist  attributes got from voms servers
    @param extra  some extra information got from voms servers
    @return true iff successfully got the attributes, otherwise false
    */
    //bool GetVOMSAttribute(std::list<std::string> voms_servers, std::list<AC*> aclist, std::string extra);


  private:
    // PKI files
    std::string cacertfile_;
    std::string cacertdir_;
    std::string certfile_;
    std::string keyfile_;

    // Verify context 
    cert_verify_context verify_ctx_;

    //Certificate structures
    X509 *           cert_;    //cert
    certType         cert_type_;
    EVP_PKEY *       pkey_;    //private key
    //int              keybits_; 
    STACK_OF(X509) * cert_chain_;  //cert chain
    PROXYCERTINFO*   proxy_cert_info_;
    //std::list<X509*> certs_;
    Credformat       format;
    Arc::Time             start_;
    Arc::Period           lifetime_;

    //Certificate/Proxy certificate request
    X509_REQ* req_;
    RSA* rsa_key_;
    EVP_MD* signing_alg_;
    int keybits_;
    int init_prime_;

    //Extensions for certificate, such as certificate policy, attributes, etc/
    STACK_OF(X509_EXTENSION)* extensions_;

    //some properties and AC settings */
    int            bits;
    //Period         period; //lifetime of certificate
    Arc::Time           ac_start;
    Arc::Period         ac_lifetime; //lifetime of attribute
    std::string    policyfile;
    std::string    policylang;
    int            pathlength;

  };

}// namespace ArcLib

#endif /* __ARC_CREDENTIAL_H__ */


