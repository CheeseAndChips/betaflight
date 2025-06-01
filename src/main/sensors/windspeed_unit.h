#pragma once

#include "build/build_config.h"
#include "windspeed.h"

STATIC_UNIT_TESTED uint8_t windspeedComputeChecksum(const char *buffer);
STATIC_UNIT_TESTED uint16_t windspeedDecodeChecksum(const windspeedEncodedChecksum_t checksum);
STATIC_UNIT_TESTED windspeedParseError_t windspeedParseResponse(char *buffer, windspeedParsedResponse_t *parsed);

STATIC_UNIT_TESTED bool windspeedPrepareCommand(const windspeedEncodedID_t id, const char *payload);
