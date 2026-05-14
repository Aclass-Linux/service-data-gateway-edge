#include <cassert>

#include "DataGatewayHub/Core/Logger.h"

int main() {
  DataGatewayHub::Core::Logger::info("logger smoke test");
  assert(true);
  return 0;
}

