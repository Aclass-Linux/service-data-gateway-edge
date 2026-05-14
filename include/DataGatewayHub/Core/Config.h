#pragma once

#include <string>
#include <unordered_map>

namespace DataGatewayHub::Core {

class Config {
public:
  void set(const std::string& key, const std::string& value);
  std::string get(const std::string& key, const std::string& fallback = "") const;

private:
  std::unordered_map<std::string, std::string> values_;
};

}  // namespace DataGatewayHub::Core

