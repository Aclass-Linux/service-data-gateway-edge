#include "DataGatewayEdge/Protocol/Packet.h"

namespace DataGatewayEdge::Protocol {

Packet::Packet(std::vector<std::uint8_t> payload) : payload_(std::move(payload)) {}

const std::vector<std::uint8_t>& Packet::payload() const {
  return payload_;
}

}  // namespace DataGatewayEdge::Protocol
