#pragma once

#include <cstddef>

namespace DataGatewayHub::Data {

class ConnectionPool {
public:
  explicit ConnectionPool(std::size_t size = 1);

  std::size_t size() const;

private:
  std::size_t size_;
};

}  // namespace DataGatewayHub::Data

