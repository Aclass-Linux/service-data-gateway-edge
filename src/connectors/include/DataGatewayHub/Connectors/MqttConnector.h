#pragma once

#include <string>

namespace DataGatewayHub::Connectors {

class MqttConnector {
public:
  bool publish(const std::string& topic, const std::string& payload) const;
};

}  // namespace DataGatewayHub::Connectors

