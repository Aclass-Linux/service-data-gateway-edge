# DataGatewayHub

## Language

**egw_err_t**:
项目全局错误码类型，`typedef int32_t`。0 (`EGW_OK`) = 成功，负值 = 具体错误原因。采用 `#define` 常量而非 C `enum`，以保证跨平台 ABI 定宽。
_Avoid_: enum, int
