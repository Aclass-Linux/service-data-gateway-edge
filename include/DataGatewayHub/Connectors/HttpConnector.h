#pragma once

#include <string>

namespace DataGatewayHub::Connectors {

class HttpConnector {
public:
  std::string get(const std::string& endpoint) const;
};

}  // namespace DataGatewayHub::Connectors

