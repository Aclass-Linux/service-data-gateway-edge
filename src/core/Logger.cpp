#include "DataGatewayEdge/Core/Logger.h"

#include <iostream>

namespace DataGatewayEdge::Core {

void Logger::info(const std::string& message) {
  std::cout << "[INFO] " << message << '\n';
}

}  // namespace DataGatewayEdge::Core
