#pragma once

#include <string>

namespace DataGatewayEdge::Hub {

class DdlExecutor {
public:
  std::string execute(const std::string& statement) const;
};

}  // namespace DataGatewayEdge::Hub
