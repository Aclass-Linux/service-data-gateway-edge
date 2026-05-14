#pragma once

#include <string>

namespace DataGatewayEdge::Core {

class Logger {
public:
  static void info(const std::string& message);
};

}  // namespace DataGatewayEdge::Core
