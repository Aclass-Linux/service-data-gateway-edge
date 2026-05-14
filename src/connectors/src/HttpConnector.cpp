#include "DataGatewayHub/Connectors/HttpConnector.h"

namespace DataGatewayHub::Connectors {

std::string HttpConnector::get(const std::string& endpoint) const {
  return "GET " + endpoint;
}

}  // namespace DataGatewayHub::Connectors

