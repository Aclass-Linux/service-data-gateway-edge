#pragma once

#include <string>

namespace DataGatewayEdge::Connectors {

class HttpConnector {
public:
  std::string get(const std::string& endpoint) const;
};

}  // namespace DataGatewayEdge::Connectors
