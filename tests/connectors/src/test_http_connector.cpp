#include <cassert>

#include "DataGatewayHub/Connectors/HttpConnector.h"

int main() {
  DataGatewayHub::Connectors::HttpConnector connector;
  assert(connector.get("/health") == "GET /health");
  return 0;
}

