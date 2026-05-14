#include "DataGatewayHub/Hub/DdlExecutor.h"

namespace DataGatewayHub::Hub {

std::string DdlExecutor::execute(const std::string& statement) const {
  return "Executed: " + statement;
}

}  // namespace DataGatewayHub::Hub

