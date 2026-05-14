#pragma once

#include <string>
#include <vector>

namespace DataGatewayEdge::Data {

class Repository {
public:
  void save(const std::string& record);
  const std::vector<std::string>& records() const;

private:
  std::vector<std::string> records_;
};

}  // namespace DataGatewayEdge::Data
