#pragma once

#include <cstdint>
#include <vector>

#include "DataGatewayEdge/Protocol/Packet.h"

namespace DataGatewayEdge::Protocol {

class FrameBuilder {
public:
  std::vector<std::uint8_t> build(const Packet& packet) const;
};

}  // namespace DataGatewayEdge::Protocol
