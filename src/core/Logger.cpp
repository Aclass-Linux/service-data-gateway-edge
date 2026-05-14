#include "DataGatewayHub/Core/Logger.h"

#include <iostream>

namespace DataGatewayHub::Core {

void Logger::info(const std::string& message) {
  std::cout << "[INFO] " << message << '\n';
}

}  // namespace DataGatewayHub::Core

