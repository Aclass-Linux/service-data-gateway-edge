#pragma once

#include <string>

namespace DataGatewayEdge::Connectors {

class MqttConnector {
public:
  bool publish(const std::string& topic, const std::string& payload) const;
};

}  // namespace DataGatewayEdge::Connectors
