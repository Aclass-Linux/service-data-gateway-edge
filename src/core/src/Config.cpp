#include "DataGatewayHub/Core/Config.h"

namespace DataGatewayHub::Core {

void Config::set(const std::string& key, const std::string& value) {
  values_[key] = value;
}

std::string Config::get(const std::string& key, const std::string& fallback) const {
  const auto it = values_.find(key);
  return it == values_.end() ? fallback : it->second;
}

}  // namespace DataGatewayHub::Core

