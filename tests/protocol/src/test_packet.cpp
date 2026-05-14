#include <cassert>
#include <cstdint>
#include <vector>

#include "DataGatewayEdge/Protocol/Packet.h"

int main() {
  const std::vector<std::uint8_t> payload{1, 2, 3};
  DataGatewayEdge::Protocol::Packet packet(payload);
  assert(packet.payload().size() == 3);
  return 0;
}
