#include "DataGatewayHub/Data/Repository.h"

namespace DataGatewayHub::Data {

void Repository::save(const std::string& record) {
  records_.push_back(record);
}

const std::vector<std::string>& Repository::records() const {
  return records_;
}

}  // namespace DataGatewayHub::Data

