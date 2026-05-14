#pragma once

#include <cstdint>
#include <vector>

namespace DataGatewayEdge::Protocol {

class Packet {
public:
  explicit Packet(std::vector<std::uint8_t> payload = {});

  const std::vector<std::uint8_t>& payload() const;

private:
  std::vector<std::uint8_t> payload_;
};

}  // namespace DataGatewayEdge::Protocol
