#include <iostream>

#include "DataGatewayHub/Hub/HubEngine.h"

int main() {
  DataGatewayHub::Hub::HubEngine engine;
  std::cout << engine.start() << '\n';
  return 0;
}

