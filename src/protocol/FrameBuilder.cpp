#include "DataGatewayHub/Protocol/FrameBuilder.h"

namespace DataGatewayHub::Protocol {

std::vector<std::uint8_t> FrameBuilder::build(const Packet& packet) const {
  return packet.payload();
}

}  // namespace DataGatewayHub::Protocol

