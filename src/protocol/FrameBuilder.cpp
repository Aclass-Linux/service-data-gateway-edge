#include "DataGatewayEdge/Protocol/FrameBuilder.h"

namespace DataGatewayEdge::Protocol {

std::vector<std::uint8_t> FrameBuilder::build(const Packet& packet) const {
  return packet.payload();
}

}  // namespace DataGatewayEdge::Protocol
