// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glibmm.h>

#include <arc/StringConv.h>
#include <arc/XMLNode.h>
#include <arc/URL.h>
#include <arc/Logger.h>
#include <arc/client/JobDescription.h>

#include "JDLParser.h"

#define ADDJDLSTRING(X, Y) (!(X).empty() ? "  " Y " = \"" + X + "\";\n" : "")
#define ADDJDLNUMBER(X, Y) ((X) > -1 ? "  " Y " = \"" + tostring(X) + "\";\n" : "")

namespace Arc {

  JDLParser::JDLParser()
    : JobDescriptionParser() {
    supportedLanguages.push_back("egee:jdl");
  }

  JDLParser::~JDLParser() {}

  Plugin* JDLParser::Instance(PluginArgument *arg) {
    return new JDLParser();
  }

  bool JDLParser::splitJDL(const std::string& original_string,
                           std::list<std::string>& lines) const {
    // Clear the return variable
    lines.clear();

    std::string jdl_text = original_string;

    bool quotation = false;
    std::list<char> stack;
    std::string actual_line;

    for (int i = 0; i < jdl_text.size() - 1; i++) {
      // Looking for control character marks the line end
      if (jdl_text[i] == ';' && !quotation && stack.empty()) {
        lines.push_back(actual_line);
        actual_line.clear();
        continue;
      }
      else if (jdl_text[i] == ';' && !quotation && stack.back() == '{') {
        logger.msg(ERROR, "[JDLParser] Semicolon (;) is not allowed inside brackets, at '%s;'.", actual_line);
        return false;
      }
      // Determinize the quotations
      if (jdl_text[i] == '"') {
        if (!quotation)
          quotation = true;
        else if (jdl_text[i - 1] != '\\')
          quotation = false;
      }
      else if (!quotation) {
        if (jdl_text[i] == '{' || jdl_text[i] == '[')
          stack.push_back(jdl_text[i]);
        else if (jdl_text[i] == '}') {
          if (stack.back() == '{')
            stack.pop_back();
          else
            return false;
        }
        else if (jdl_text[i] == ']') {
          if (stack.back() == '[')
            stack.pop_back();
          else
            return false;
        }
      }
      actual_line += jdl_text[i];
    }
    return true;
  }

  std::string JDLParser::simpleJDLvalue(const std::string& attributeValue) const {
    const std::string whitespaces(" \t\f\v\n\r");
    const size_t last_pos = attributeValue.find_last_of("\"");

    // If the text is not between quotation marks, then return with the original form
    if (attributeValue.substr(attributeValue.find_first_not_of(whitespaces), 1) != "\"" || last_pos == std::string::npos)
      return trim(attributeValue);
    // Else remove the marks and return with the quotation's content
    else
      return attributeValue.substr(attributeValue.find_first_of("\"") + 1, last_pos - attributeValue.find_first_of("\"") - 1);
  }

  std::list<std::string> JDLParser::listJDLvalue(const std::string& attributeValue, std::pair<char, char> brackets, char lineEnd) const {
    std::list<std::string> elements;
    unsigned long first_bracket = attributeValue.find_first_of(brackets.first);
    if (first_bracket == std::string::npos) {
      elements.push_back(simpleJDLvalue(attributeValue));
      return elements;
    }
    unsigned long last_bracket = attributeValue.find_last_of(brackets.second);
    if (last_bracket == std::string::npos) {
      elements.push_back(simpleJDLvalue(attributeValue));
      return elements;
    }
    std::list<std::string> listElements;
    tokenize(attributeValue.substr(first_bracket + 1,
                                   last_bracket - first_bracket - 1),
             listElements, tostring(lineEnd));
    for (std::list<std::string>::const_iterator it = listElements.begin();
         it != listElements.end(); it++)
      elements.push_back(simpleJDLvalue(*it));
    return elements;
  }

  bool JDLParser::handleJDLattribute(const std::string& attributeName_,
                                     const std::string& attributeValue,
                                     JobDescription& job) const {
    // To do the attributes name case-insensitive do them lowercase and remove the quotiation marks
    std::string attributeName = lower(attributeName_);
    if (attributeName == "type") {
      std::string value = lower(simpleJDLvalue(attributeValue));
      if (value == "job")
        return true;
      if (value == "dag") {
        logger.msg(VERBOSE, "[JDLParser] This kind of JDL decriptor is not supported yet: %s", value);
        return false;  // This kind of JDL decriptor is not supported yet
      }
      if (value == "collection") {
        logger.msg(VERBOSE, "[JDLParser] This kind of JDL decriptor is not supported yet: %s", value);
        return false;  // This kind of JDL decriptor is not supported yet
      }
      logger.msg(VERBOSE, "[JDLParser] Attribute name: %s, has unknown value: %s", attributeName, value);
      return false;    // Unknown attribute value - error
    }
    else if (attributeName == "jobtype")
      return true;     // Skip this attribute
    else if (attributeName == "executable") {
      job.Application.Executable.Name = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "arguments") {
      tokenize(simpleJDLvalue(attributeValue), job.Application.Executable.Argument);
      return true;
    }
    else if (attributeName == "stdinput") {
      job.Application.Input = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "stdoutput") {
      job.Application.Output = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "stderror") {
      job.Application.Error = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "inputsandbox") {
      std::list<std::string> inputfiles = listJDLvalue(attributeValue);
      for (std::list<std::string>::const_iterator it = inputfiles.begin();
           it != inputfiles.end(); it++) {
        FileType file;
        const std::size_t pos = it->find_last_of('/');
        file.Name = (pos == std::string::npos ? *it : it->substr(pos+1));
        file.Source.push_back(URL(*it));
        if (!file.Source.back()) {
          return false;
        }
        // Initializing these variables
        file.KeepData = false;
        file.IsExecutable = false;
        job.Files.push_back(file);
      }
      return true;
    }
    else if (attributeName == "inputsandboxbaseuri") {
      for (std::list<FileType>::iterator it = job.Files.begin();
           it != job.Files.end(); it++) {
        /* Since JDL does not have support for multiple locations the size of
         * the Source member is exactly 1.
         */
        if (!it->Source.empty() && !it->Source.front()) {
          it->Source.front() = simpleJDLvalue(attributeValue);
        }
      }
      return true;
    }
    else if (attributeName == "outputsandbox") {
      std::list<std::string> outputfiles = listJDLvalue(attributeValue);
      for (std::list<std::string>::const_iterator it = outputfiles.begin();
           it != outputfiles.end(); it++) {
        FileType file;
        file.Name = *it;
        file.Target.push_back(URL(*it));
        if (!file.Target.back()) {
          return false;
        }
        // Initializing these variables
        file.KeepData = false;
        file.IsExecutable = false;
        job.Files.push_back(file);
      }
      return true;
    }
    else if (attributeName == "outputsandboxdesturi") {
      std::list<std::string> value = listJDLvalue(attributeValue);
      std::list<std::string>::iterator i = value.begin();
      for (std::list<FileType>::iterator it = job.Files.begin();
           it != job.Files.end(); it++) {
        if (it->Target.empty()) {
          continue;
        }
        if (i != value.end()) {
          URL url = *i;
          if (url.Protocol() == "gsiftp" && url.Host() == "localhost") {
            /* Specifying the local grid ftp server (local to CREAM),
             * is the "same", in ARC analogy, to specify the output
             * files being user downloadable files. Upon finished job
             * execution CREAM will copy outputfiles to the specified
             * destination, it does not support storing them at the
             * working directory of the job for later retrieval. Instead
             * the local grid ftp server to CREAM can be specified.
             */
            it->Target.clear();
            it->KeepData = true;
          }
          else {
            it->Target.front() = url;
          }
          i++;
        }
        else {
          logger.msg(VERBOSE, "Not enough outputsandboxdesturi elements!");
          return false;
        }
      }
      return true;
    }
/*
 * The parsing of the outputsandboxbasedesturi does not work as intended.
 * Either it should be unsupported (which it is now) or else it should
 * be implemented correctly.
    else if (attributeName == "outputsandboxbasedesturi") {
      for (std::list<FileType>::iterator it = job.Files.begin();
           it != job.Files.end(); it++)
        if (!it->Target.empty() && !it->Target.front()) {
          it->Target.front() = simpleJDLvalue(attributeValue);
      return true;
    }
*/
    else if (attributeName == "prologue") {
      job.Application.Prologue.Name = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "prologuearguments") {
      tokenize(simpleJDLvalue(attributeValue), job.Application.Prologue.Argument);
      return true;
    }
    else if (attributeName == "epilogue") {
      job.Application.Epilogue.Name = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "epiloguearguments") {
      tokenize(simpleJDLvalue(attributeValue), job.Application.Epilogue.Argument);
      return true;
    }
    else if (attributeName == "allowzippedisb") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;AllowZippedISB"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "zippedisb") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;ZippedISB"] = "\"" + simpleJDLvalue(attributeValue) + "\"";
      return true;
    }
    else if (attributeName == "expirytime") {
      job.Application.ExpiryTime = Time(stringtol(simpleJDLvalue(attributeValue)));
      return true;
    }
    else if (attributeName == "environment") {
      std::list<std::string> variables = listJDLvalue(attributeValue);
      for (std::list<std::string>::const_iterator it = variables.begin();
           it != variables.end(); it++) {
        std::string::size_type equal_pos = it->find('=');
        if (equal_pos != std::string::npos) {
          job.Application.Environment.push_back(
            std::pair<std::string, std::string>(
              trim(it->substr(0, equal_pos)),
              trim(it->substr(equal_pos + 1))));
        }
        else {
          logger.msg(VERBOSE, "[JDLParser] Environment variable has been defined without any equal sign.");
          return false;
        }
      }
      return true;
    }
    else if (attributeName == "perusalfileenable") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;PerusalFileEnable"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "perusaltimeinterval") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;PerusalTimeInterval"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "perusalfilesdesturi") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;PerusalFilesDestURI"] = "\"" + simpleJDLvalue(attributeValue) + "\"";
      return true;
    }
    else if (attributeName == "inputdata")
      // Not supported yet
      // will be soon deprecated
      return true;
    else if (attributeName == "outputdata")
      // Not supported yet
      // will be soon deprecated
      return true;
    else if (attributeName == "storageindex")
      // Not supported yet
      // will be soon deprecated
      return true;
    else if (attributeName == "datacatalog")
      // Not supported yet
      // will be soon deprecated
      return true;
    else if (attributeName == "datarequirements") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;DataRequirements"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "dataaccessprotocol") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;DataAccessProtocol"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "virtualorganisation") {
      job.Identification.JobVOName = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "queuename") {
      job.Resources.QueueName = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "batchsystem") {
      job.OtherAttributes["egee:jdl;batchsystem"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "retrycount") {
      const int count = stringtoi(simpleJDLvalue(attributeValue));
      if (job.Application.Rerun < count)
        job.Application.Rerun = count;
      return true;
    }
    else if (attributeName == "shallowretrycount") {
      const int count = stringtoi(simpleJDLvalue(attributeValue));
      if (job.Application.Rerun < count)
        job.Application.Rerun = count;
      return true;
    }
    else if (attributeName == "lbaddress") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;LBAddress"] = "\"" + simpleJDLvalue(attributeValue) + "\"";
      return true;
    }
    else if (attributeName == "myproxyserver") {
      URL url(simpleJDLvalue(attributeValue));
      if (!url)
        return false;
      job.Application.CredentialService.push_back(url);
      return true;
    }
    else if (attributeName == "hlrlocation") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;HLRLocation"] = "\"" + simpleJDLvalue(attributeValue) + "\"";
      return true;
    }
    else if (attributeName == "jobprovenance") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;JobProvenance"] = "\"" + simpleJDLvalue(attributeValue) + "\"";
      return true;
    }
    else if (attributeName == "nodenumber") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;NodeNumber"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "jobsteps")
      // Not supported yet
      // will be soon deprecated
      return true;
    else if (attributeName == "currentstep")
      // Not supported yet
      // will be soon deprecated
      return true;
    else if (attributeName == "jobstate")
      // Not supported yet
      // will be soon deprecated
      return true;
    else if (attributeName == "listenerport") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;ListenerPort"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "listenerhost") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;ListenerHost"] = "\"" + simpleJDLvalue(attributeValue) + "\"";
      return true;
    }
    else if (attributeName == "listenerpipename") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;ListenerPipeName"] = "\"" + simpleJDLvalue(attributeValue) + "\"";
      return true;
    }
    else if (attributeName == "requirements") {
      // It's too complicated to determinize the right conditions, because the definition language is
      // LRMS specific.
      // Only store it.
      job.OtherAttributes["egee:jdl;Requirements"] = "\"" + simpleJDLvalue(attributeValue) + "\"";
      return true;
    }
    else if (attributeName == "rank") {
      job.OtherAttributes["egee:jdl;rank"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "fuzzyrank") {
      job.OtherAttributes["egee:jdl;fuzzyrank"] = simpleJDLvalue(attributeValue);
      return true;
    }
    else if (attributeName == "usertags") {
      job.Identification.UserTag = listJDLvalue(attributeValue, std::make_pair('[', ']'), ';');
      return true;
    }
    else if (attributeName == "outputse") {
      // Not supported yet, only store it
      job.OtherAttributes["egee:jdl;OutputSE"] = "\"" + simpleJDLvalue(attributeValue) + "\"";
      return true;
    }

    logger.msg(WARNING, "[JDL Parser]: Unknown attribute name: \'%s\', with value: %s", attributeName, attributeValue);
    return true;
  }

  std::string JDLParser::generateOutputList(const std::string& attribute, const std::list<std::string>& list, std::pair<char, char> brackets, char lineEnd) const {
    const std::string space = "             "; // 13 spaces seems to be standard padding.
    std::ostringstream output;
    output << "  " << attribute << " = " << brackets.first << std::endl;
    for (std::list<std::string>::const_iterator it = list.begin();
         it != list.end(); it++) {
      if (it != list.begin())
        output << lineEnd << std::endl;
      output << space << "\"" << *it << "\"";
    }

    output << std::endl << space << brackets.second << ";" << std::endl;
    return output.str();
  }

  bool JDLParser::Parse(const std::string& source, std::list<JobDescription>& jobdescs, const std::string& language, const std::string& dialect) const {
    if (language != "" && !IsLanguageSupported(language)) {
      return false;
    }

    logger.msg(VERBOSE, "Parsing string using JDLParser");

    jobdescs.clear();
    jobdescs.push_back(JobDescription());

    JobDescription& jobdesc = jobdescs.back();

    unsigned long first = source.find_first_of("[");
    unsigned long last = source.find_last_of("]");
    if (first == std::string::npos || last == std::string::npos) {
      logger.msg(VERBOSE, "[JDLParser] There is at least one necessary ruler character missing. ('[' or ']')");
      jobdescs.clear();
      return false;
    }
    std::string input_text = source.substr(first + 1, last - first - 1);

    //Remove multiline comments
    unsigned long comment_start = 0;
    while ((comment_start = input_text.find("/*", comment_start)) != std::string::npos)
      input_text.erase(input_text.begin() + comment_start, input_text.begin() + input_text.find("*/", comment_start) + 2);

    std::string wcpy = "";
    std::list<std::string> lines;
    tokenize(input_text, lines, "\n");
    for (std::list<std::string>::iterator it = lines.begin();
         it != lines.end();) {
      // Remove empty lines
      const std::string trimmed_line = trim(*it);
      if (trimmed_line.length() == 0)
        it = lines.erase(it);
      // Remove lines starts with '#' - Comments
      else if (trimmed_line.length() >= 1 && trimmed_line.substr(0, 1) == "#")
        it = lines.erase(it);
      // Remove lines starts with '//' - Comments
      else if (trimmed_line.length() >= 2 && trimmed_line.substr(0, 2) == "//")
        it = lines.erase(it);
      else {
        wcpy += *it + "\n";
        it++;
      }
    }

    if (!splitJDL(wcpy, lines)) {
      logger.msg(VERBOSE, "[JDLParser] Syntax error found during the split function.");
      jobdescs.clear();
      return false;
    }
    if (lines.size() <= 0) {
      logger.msg(VERBOSE, "[JDLParser] Lines count is zero or other funny error has occurred.");
      jobdescs.clear();
      return false;
    }

    for (std::list<std::string>::iterator it = lines.begin();
         it != lines.end(); it++) {
      const size_t equal_pos = it->find_first_of("=");
      if (equal_pos == std::string::npos) {
        logger.msg(VERBOSE, "[JDLParser] JDL syntax error. There is at least one equal sign missing where it would be expected.");
        jobdescs.clear();
        return false;
      }
      if (!handleJDLattribute(trim(it->substr(0, equal_pos)), trim(it->substr(equal_pos + 1)), jobdesc)) {
        jobdescs.clear();
        return false;
      }
    }

    SourceLanguage(jobdesc) = (!language.empty() ? language : supportedLanguages.front());
    logger.msg(INFO, "String successfully parsed as %s", jobdesc.GetSourceLanguage());
    return true;
  }

  bool JDLParser::UnParse(const JobDescription& job, std::string& product, const std::string& language, const std::string& dialect) const {
    if (!IsLanguageSupported(language)) {
      return false;
    }


    if (job.Application.Executable.Name.empty()) {
      return false;
    }

    product = "[\n  Type = \"job\";\n";

    product += ADDJDLSTRING(job.Application.Executable.Name, "Executable");
    if (!job.Application.Executable.Argument.empty()) {
      product += "  Arguments = \"";
      for (std::list<std::string>::const_iterator it = job.Application.Executable.Argument.begin();
           it != job.Application.Executable.Argument.end(); it++) {
        if (it != job.Application.Executable.Argument.begin())
          product += " ";
        product += *it;
      }
      product += "\";\n";
    }

    product += ADDJDLSTRING(job.Application.Input, "StdInput");
    product += ADDJDLSTRING(job.Application.Output, "StdOutput");
    product += ADDJDLSTRING(job.Application.Error, "StdError");
    product += ADDJDLSTRING(job.Identification.JobVOName, "VirtualOrganisation");

    if (!job.Application.Environment.empty()) {
      std::list<std::string> environment;
      for (std::list< std::pair<std::string, std::string> >::const_iterator it = job.Application.Environment.begin();
           it != job.Application.Environment.end(); it++) {
        environment.push_back(it->first + " = " + it->second);
      }

      if (!environment.empty())
        product += generateOutputList("Environment", environment);
    }

    product += ADDJDLSTRING(job.Application.Prologue.Name, "Prologue");
    if (!job.Application.Prologue.Argument.empty()) {
      product += "  PrologueArguments = \"";
      for (std::list<std::string>::const_iterator iter = job.Application.Prologue.Argument.begin();
           iter != job.Application.Prologue.Argument.end(); iter++) {
        if (iter != job.Application.Prologue.Argument.begin())
          product += " ";
        product += *iter;
      }
      product += "\";\n";
    }

    product += ADDJDLSTRING(job.Application.Epilogue.Name, "Epilogue");
    if (!job.Application.Epilogue.Argument.empty()) {
      product += "  EpilogueArguments = \"";
      for (std::list<std::string>::const_iterator iter = job.Application.Epilogue.Argument.begin();
           iter != job.Application.Epilogue.Argument.end(); iter++) {
        if (iter != job.Application.Epilogue.Argument.begin())
          product += " ";
        product += *iter;
      }
      product += "\";\n";
    }

    if (!job.Application.Executable.Name.empty() ||
        !job.Files.empty() ||
        !job.Application.Input.empty() ||
        !job.Application.Output.empty() ||
        !job.Application.Error.empty()) {

      bool addExecutable = !job.Application.Executable.Name.empty() && !Glib::path_is_absolute(job.Application.Executable.Name);
      bool addInput      = !job.Application.Input.empty();
      bool addOutput     = !job.Application.Output.empty();
      bool addError      = !job.Application.Error.empty();

      std::list<std::string> inputSandboxList;
      std::list<std::string> outputSandboxList;
      std::list<std::string> outputSandboxDestURIList;
      for (std::list<FileType>::const_iterator it = job.Files.begin();
           it != job.Files.end(); it++) {
        /* Since JDL does not have support for multiple locations only the first
         * location will be added.
         */
        if (!it->Source.empty())
          inputSandboxList.push_back(it->Source.front() ? it->Source.front().fullstr() : it->Name);
        if ((!it->Target.empty() && it->Target.front()) || it->KeepData) {
          outputSandboxList.push_back(it->Name);
          /* User downloadable files should go to the local grid ftp
           * server (local to CREAM). See comments on the parsing of the
           * outputsandboxdesturi attribute above.
           */
          const std::string uri_tmp = (it->Target.empty() || it->Target.front().Protocol() == "file" ?
                                       "gsiftp://localhost/" + it->Name :
                                       it->Target.front().fullstr());
          outputSandboxDestURIList.push_back(uri_tmp);
        }

        addExecutable &= (it->Name != job.Application.Executable.Name);
        addInput      &= (it->Name != job.Application.Input);
        addOutput     &= (it->Name != job.Application.Output);
        addError      &= (it->Name != job.Application.Error);
      }

      if (addExecutable)
        inputSandboxList.push_back(job.Application.Executable.Name);
      if (addInput)
        inputSandboxList.push_back(job.Application.Input);
      if (addOutput) {
        outputSandboxList.push_back(job.Application.Output);
        outputSandboxDestURIList.push_back("gsiftp://localhost/" + job.Application.Output);
      } 
      if (addError) {
        outputSandboxList.push_back(job.Application.Error);
        outputSandboxDestURIList.push_back("gsiftp://localhost/" + job.Application.Error);
      }

      if (!inputSandboxList.empty())
        product += generateOutputList("InputSandbox", inputSandboxList);
      if (!outputSandboxList.empty())
        product += generateOutputList("OutputSandbox", outputSandboxList);
      if (!outputSandboxDestURIList.empty())
        product += generateOutputList("OutputSandboxDestURI", outputSandboxDestURIList);
    }

    if (!job.Resources.QueueName.empty()) {
      product += "  QueueName = \"";
      product += job.Resources.QueueName;
      product += "\";\n";
    }

    product += ADDJDLNUMBER(job.Application.Rerun, "RetryCount");
    product += ADDJDLNUMBER(job.Application.Rerun, "ShallowRetryCount");
    product += ADDJDLNUMBER(job.Application.ExpiryTime.GetTime(), "ExpiryTime");

    if (!job.Application.CredentialService.empty() &&
        job.Application.CredentialService.front()) {
      product += "  MyProxyServer = \"";
      product += job.Application.CredentialService.front().fullstr();
      product += "\";\n";
    }

    if (!job.Identification.UserTag.empty())
      product += generateOutputList("UserTags", job.Identification.UserTag, std::pair<char, char>('[', ']'), ';');

    if (!job.OtherAttributes.empty()) {
      std::map<std::string, std::string>::const_iterator it;
      for (it = job.OtherAttributes.begin(); it != job.OtherAttributes.end(); it++) {
        std::list<std::string> keys;
        tokenize(it->first, keys, ";");
        if (keys.size() != 2 || keys.front() != "egee:jdl") {
          continue;
        }
        product += "  ";
        product += keys.back();
        product += " = \"";
        product += it->second;
        product += "\";\n";
      }
    }
    product += "]";

    return true;
  }

} // namespace Arc
