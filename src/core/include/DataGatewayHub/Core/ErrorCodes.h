#pragma once

namespace DataGatewayHub::Core {

enum class ErrorCode {
  Ok = 0,
  InvalidArgument = 1,
  ConnectionFailed = 2,
  Unknown = 999
};

const char* toString(ErrorCode code);

}  // namespace DataGatewayHub::Core

