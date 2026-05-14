#include "DataGatewayEdge/Data/ConnectionPool.h"

namespace DataGatewayEdge::Data {

ConnectionPool::ConnectionPool(std::size_t size) : size_(size) {}

std::size_t ConnectionPool::size() const {
  return size_;
}

}  // namespace DataGatewayEdge::Data
