#pragma once

#include <cstdint>
#include <vector>

#include "DataGatewayHub/Protocol/Packet.h"

namespace DataGatewayHub::Protocol {

class FrameBuilder {
public:
  std::vector<std::uint8_t> build(const Packet& packet) const;
};

}  // namespace DataGatewayHub::Protocol

