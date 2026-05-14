#include "DataGatewayHub/Data/ConnectionPool.h"

namespace DataGatewayHub::Data {

ConnectionPool::ConnectionPool(std::size_t size) : size_(size) {}

std::size_t ConnectionPool::size() const {
  return size_;
}

}  // namespace DataGatewayHub::Data

