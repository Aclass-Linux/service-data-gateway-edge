#include <cassert>

#include "DataGatewayHub/Hub/HubEngine.h"

int main() {
  DataGatewayHub::Hub::HubEngine engine;
  assert(engine.start() == "HubEngine started");
  return 0;
}

