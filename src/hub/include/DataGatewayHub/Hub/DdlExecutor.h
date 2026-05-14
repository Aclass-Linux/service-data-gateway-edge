#pragma once

#include <string>

namespace DataGatewayHub::Hub {

class DdlExecutor {
public:
  std::string execute(const std::string& statement) const;
};

}  // namespace DataGatewayHub::Hub

