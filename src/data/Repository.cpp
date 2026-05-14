#include "DataGatewayEdge/Data/Repository.h"

namespace DataGatewayEdge::Data {

void Repository::save(const std::string& record) {
  records_.push_back(record);
}

const std::vector<std::string>& Repository::records() const {
  return records_;
}

}  // namespace DataGatewayEdge::Data
