#include "DataGatewayEdge/Connectors/HttpConnector.h"

namespace DataGatewayEdge::Connectors {

std::string HttpConnector::get(const std::string& endpoint) const {
  return "GET " + endpoint;
}

}  // namespace DataGatewayEdge::Connectors
