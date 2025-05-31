#pragma once

#include "build/build_config.h"

#include <stddef.h>
#include <string.h>
#include <stdint.h>

typedef enum {
    windspeedParseOk = 0,
    windspeedParseNoDollar,
    windspeedParseNoComma,
    windspeedParseNoAsterisk,
    windspeedParseNoCRLF,
    windspeedParseBadIDLength,
    windspeedParseBadChecksumLength,
} windspeedParseError_t;

const uint16_t WINDSPEED_INVALID_CHECKSUM = 0xffff;

typedef char windspeedEncodedID_t[2];
typedef char windspeedEncodedChecksum_t[2];

typedef struct {
    char *payload_start;
    windspeedEncodedID_t id;
    windspeedEncodedChecksum_t checksum;
} windspeedParsedResponse_t;

STATIC_UNIT_TESTED uint8_t windspeedComputeChecksum(const char *buffer);
STATIC_UNIT_TESTED uint16_t windspeedDecodeChecksum(const windspeedEncodedChecksum_t checksum);
STATIC_UNIT_TESTED windspeedParseError_t windspeedParseResponse(char *buffer, windspeedParsedResponse_t *parsed);
