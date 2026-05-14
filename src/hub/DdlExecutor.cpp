#include "DataGatewayEdge/Hub/DdlExecutor.h"

namespace DataGatewayEdge::Hub {

std::string DdlExecutor::execute(const std::string& statement) const {
  return "Executed: " + statement;
}

}  // namespace DataGatewayEdge::Hub
