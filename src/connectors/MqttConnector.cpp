#include "DataGatewayEdge/Connectors/MqttConnector.h"

namespace DataGatewayEdge::Connectors {

bool MqttConnector::publish(const std::string& topic, const std::string& payload) const {
  return !topic.empty() && !payload.empty();
}

}  // namespace DataGatewayEdge::Connectors
