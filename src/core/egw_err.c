#include "egw_defs.h"

const char *egw_err_str(egw_err_t err) {
    switch (err) {
        #define EGW_ERROR_CODE(name, val, desc) case EGW_##name: return "EGW_" #name " (" #val "): " desc;
        #include "egw_err.inc"
        default: return "EGW_UNKNOWN";
    }
}
