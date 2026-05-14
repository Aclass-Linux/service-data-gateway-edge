#include <cassert>

#include "DataGatewayEdge/Core/Logger.h"

int main() {
  DataGatewayEdge::Core::Logger::info("logger smoke test");
  assert(true);
  return 0;
}
