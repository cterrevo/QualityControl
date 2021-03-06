// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file   InfrastructureGenerator.cxx
/// \author Piotr Konopka
///

#include "QualityControl/InfrastructureGenerator.h"

#include "QualityControl/CheckerFactory.h"
#include "QualityControl/HistoMerger.h"
#include "QualityControl/TaskRunner.h"
#include "QualityControl/TaskRunnerFactory.h"
#include "QualityControl/Version.h"
#include "QualityControl/QcInfoLogger.h"

#include <boost/property_tree/ptree.hpp>
#include <Configuration/ConfigurationFactory.h>

using namespace o2::framework;
using namespace o2::configuration;
using namespace o2::quality_control::checker;
using boost::property_tree::ptree;

namespace o2::quality_control::core
{

WorkflowSpec InfrastructureGenerator::generateLocalInfrastructure(std::string configurationSource, std::string host)
{
  WorkflowSpec workflow;
  TaskRunnerFactory taskRunnerFactory;
  auto config = ConfigurationFactory::getConfiguration(configurationSource);
  printVersion();

  for (const auto& [taskName, taskConfig] : config->getRecursive("qc.tasks")) {
    if (taskConfig.get<bool>("active") && taskConfig.get<std::string>("location") == "local") {
      // ids are assigned to local tasks in order to distinguish monitor objects outputs from each other and be able to
      // merge them. If there is no need to merge (only one qc task), it gets subspec 0.
      // todo: use matcher for subspec when available in DPL
      if (host.empty()) {
        workflow.emplace_back(taskRunnerFactory.create(taskName, configurationSource, 0, false));
      } else {
        size_t id = taskConfig.get_child("machines").size() > 1 ? 1 : 0;
        for (const auto& machine : taskConfig.get_child("machines")) {

          if (machine.second.get<std::string>("") == host) {
            // todo: optimize it by using the same ptree?
            workflow.emplace_back(taskRunnerFactory.create(taskName, configurationSource, id, true));
            break;
          }
          id++;
        }
      }
    }
  }
  return workflow;
}

void InfrastructureGenerator::generateLocalInfrastructure(framework::WorkflowSpec& workflow, std::string configurationSource, std::string host)
{
  auto qcInfrastructure = InfrastructureGenerator::generateLocalInfrastructure(configurationSource, host);
  workflow.insert(std::end(workflow), std::begin(qcInfrastructure), std::end(qcInfrastructure));
}

o2::framework::WorkflowSpec InfrastructureGenerator::generateRemoteInfrastructure(std::string configurationSource)
{
  WorkflowSpec workflow;
  auto config = ConfigurationFactory::getConfiguration(configurationSource);
  printVersion();

  TaskRunnerFactory taskRunnerFactory;
  CheckerFactory checkerFactory;
  for (const auto& [taskName, taskConfig] : config->getRecursive("qc.tasks")) {
    // todo sanitize somehow this if-frenzy
    if (taskConfig.get<bool>("active", true)) {
      if (taskConfig.get<std::string>("location") == "local") {
        // if tasks are LOCAL, generate mergers + checkers

        //todo use real mergers when they are done

        // generate merger only, when there is a need to merge something
        if (taskConfig.get_child("machines").size() > 1) {
          HistoMerger merger(taskName + "-merger", 1);
          merger.configureInputsOutputs(TaskRunner::createTaskDataOrigin(),
                                        TaskRunner::createTaskDataDescription(taskName),
                                        { 1, taskConfig.get_child("machines").size() });
          DataProcessorSpec mergerSpec{
            merger.getName(),
            merger.getInputSpecs(),
            Outputs{ merger.getOutputSpec() },
            adaptFromTask<HistoMerger>(std::move(merger)),
          };

          workflow.emplace_back(mergerSpec);
        }

      } else if (taskConfig.get<std::string>("location") == "remote") {
        // -- if tasks are REMOTE, generate tasks + mergers + checkers

        workflow.emplace_back(taskRunnerFactory.create(taskName, configurationSource, 0));
      }

      workflow.emplace_back(checkerFactory.create(taskName + "-checker", taskName, configurationSource));
    }
  }
  return workflow;
}

void InfrastructureGenerator::generateRemoteInfrastructure(framework::WorkflowSpec& workflow, std::string configurationSource)
{
  auto qcInfrastructure = InfrastructureGenerator::generateRemoteInfrastructure(configurationSource);
  workflow.insert(std::end(workflow), std::begin(qcInfrastructure), std::end(qcInfrastructure));
}

void InfrastructureGenerator::customizeInfrastructure(std::vector<framework::CompletionPolicy>& policies)
{
  TaskRunnerFactory::customizeInfrastructure(policies);
}

void InfrastructureGenerator::printVersion()
{
  // Log the version number
  QcInfoLogger::GetInstance() << "QC version " << o2::quality_control::core::Version::getString() << infologger::endm;
}

} // namespace o2::quality_control::core