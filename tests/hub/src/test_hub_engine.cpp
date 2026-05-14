#include <cassert>

#include "DataGatewayEdge/Hub/HubEngine.h"

int main() {
  DataGatewayEdge::Hub::HubEngine engine;
  assert(engine.start() == "HubEngine started");
  return 0;
}
