#include "pch.h"
#include "log.h"

NvAPI_Status Ok(const char* function_name)
{
    LOG_TRACE_FAKENVAPI("{}: {}", function_name, "OK");
    return NVAPI_OK;
}

NvAPI_Status Error(const char* function_name, NvAPI_Status status)
{
    LOG_TRACE_FAKENVAPI("{}: {}", function_name, from_error_nr(status));
    return status;
}
