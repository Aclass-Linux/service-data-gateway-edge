#include <cassert>

#include "DataGatewayEdge/Connectors/HttpConnector.h"

int main() {
  DataGatewayEdge::Connectors::HttpConnector connector;
  assert(connector.get("/health") == "GET /health");
  return 0;
}
