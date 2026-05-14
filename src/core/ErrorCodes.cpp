#include "DataGatewayHub/Core/ErrorCodes.h"

namespace DataGatewayHub::Core {

const char* toString(ErrorCode code) {
  switch (code) {
    case ErrorCode::Ok:
      return "Ok";
    case ErrorCode::InvalidArgument:
      return "InvalidArgument";
    case ErrorCode::ConnectionFailed:
      return "ConnectionFailed";
    case ErrorCode::Unknown:
    default:
      return "Unknown";
  }
}

}  // namespace DataGatewayHub::Core

