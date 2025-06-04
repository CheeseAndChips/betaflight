#pragma once

#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "common/time.h"

#define WINDSPEED_TX_BUFFER_SIZE (64)

typedef enum {
    windspeedParseOk = 0,
    windspeedParseNoDollar,
    windspeedParseNoComma,
    windspeedParseNoAsterisk,
    windspeedParseNoCRLF,
    windspeedParseBadIDLength,
    windspeedParseBadChecksumLength,
} windspeedParseError_t;

#define WINDSPEED_INVALID_CHECKSUM (0xffff)

typedef char windspeedEncodedID_t[2];
typedef char windspeedEncodedChecksum_t[2];

typedef struct {
    char *payload_start;
    windspeedEncodedID_t id;
    windspeedEncodedChecksum_t checksum;
} windspeedParsedResponse_t;

typedef struct {
    char buffer[WINDSPEED_TX_BUFFER_SIZE];
    size_t buffer_filled;
} windspeedTxBuffer_t;

void windspeedInit(void);
void windspeedUpdate(timeUs_t currentTimeUs);
