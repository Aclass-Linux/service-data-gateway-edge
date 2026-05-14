#pragma once

#include <string>

namespace DataGatewayHub::Core {

class Logger {
public:
  static void info(const std::string& message);
};

}  // namespace DataGatewayHub::Core

