#include "DataGatewayHub/Connectors/MqttConnector.h"

namespace DataGatewayHub::Connectors {

bool MqttConnector::publish(const std::string& topic, const std::string& payload) const {
  return !topic.empty() && !payload.empty();
}

}  // namespace DataGatewayHub::Connectors

