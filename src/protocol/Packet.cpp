#include "DataGatewayHub/Protocol/Packet.h"

namespace DataGatewayHub::Protocol {

Packet::Packet(std::vector<std::uint8_t> payload) : payload_(std::move(payload)) {}

const std::vector<std::uint8_t>& Packet::payload() const {
  return payload_;
}

}  // namespace DataGatewayHub::Protocol

