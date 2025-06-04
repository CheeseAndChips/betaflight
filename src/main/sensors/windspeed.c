#include "windspeed.h"
#include "windspeed_unit.h"
#include <stdio.h>

STATIC_UNIT_TESTED windspeedTxBuffer_t windspeedTxBuffer;

STATIC_UNIT_TESTED uint8_t windspeedComputeChecksum(const char *buffer) {
    uint8_t result = 0;
    while (*buffer != '\0') {
        result ^= *buffer++;
    }
    return result;
}

static uint16_t windspeedParseChecksumCharacter(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return WINDSPEED_INVALID_CHECKSUM;
}

static char windspeedEncodeCharacter(uint8_t c) {
    if (c < 10) return c + '0';
    if (c < 16) return c - 10 + 'A';
    return 'X';
}

static void windspeedEncodeChecksum(uint8_t checksum, windspeedEncodedChecksum_t checksum_out) {
    checksum_out[0] = windspeedEncodeCharacter((checksum >> 4) & 0xf);
    checksum_out[1] = windspeedEncodeCharacter(checksum & 0xf);
}

STATIC_UNIT_TESTED uint16_t windspeedDecodeChecksum(const windspeedEncodedChecksum_t checksum) {
    uint16_t char1 = windspeedParseChecksumCharacter(checksum[0]);
    uint16_t char2 = windspeedParseChecksumCharacter(checksum[1]);
    if (char1 == WINDSPEED_INVALID_CHECKSUM || char2 == WINDSPEED_INVALID_CHECKSUM) {
        return WINDSPEED_INVALID_CHECKSUM;
    }
    return char1 << 4 | char2;
}

/// Modifies `buffer`, inserts single null byte
/// to turn the payload into a C-string
STATIC_UNIT_TESTED windspeedParseError_t windspeedParseResponse(char *buffer, windspeedParsedResponse_t *parsed) {
    char *dollar = strchr(buffer, '$');
    if (dollar == NULL) {
        return windspeedParseNoDollar;
    }
    char *comma = strchr(dollar, ',');
    if (comma == NULL) {
        return windspeedParseNoComma;
    }
    char *asterisk = strchr(comma, '*');
    if (asterisk == NULL) {
        return windspeedParseNoAsterisk;
    }
    char *crlf = strstr(asterisk, "\r\n");
    if (crlf == NULL) {
        return windspeedParseNoCRLF;
    }
    if (comma - dollar != sizeof(parsed->id) + 1) {
        return windspeedParseBadIDLength;
    }
    if (crlf - asterisk != sizeof(parsed->checksum) + 1) {
        return windspeedParseBadChecksumLength;
    }
    parsed->payload_start = comma + 1;
    *asterisk = '\0';
    memcpy(parsed->id, dollar + 1, sizeof(parsed->id));
    memcpy(parsed->checksum, asterisk + 1, sizeof(parsed->checksum));
    return windspeedParseOk;
}

static bool windspeedPushChar(char **buffer, int *bufferRemaining, char c) {
    if (*bufferRemaining <= 0) {
        return false;
    }
    **buffer = c;
    ++(*buffer);
    --(*bufferRemaining);
    return true;
}

static bool windspeedPushStringLen(char **buffer, int *bufferRemaining, const char *string, int stringLen) {
    for (int i = 0; i < stringLen; i++) {
        if (!windspeedPushChar(buffer, bufferRemaining, string[i])) {
            return false;
        }
    }
    return true;
}

static bool windspeedPushString(char **buffer, int *bufferRemaining, const char *string) {
    return windspeedPushStringLen(buffer, bufferRemaining, string, strlen(string));
}

STATIC_UNIT_TESTED bool windspeedPrepareCommand(const windspeedEncodedID_t id, const char *payload) {
    windspeedEncodedChecksum_t checksum_encoded;
    int bufferRemaining = WINDSPEED_TX_BUFFER_SIZE;
    char *buffer = windspeedTxBuffer.buffer;
    if (!windspeedPushChar(&buffer, &bufferRemaining, '$')) {
        return false;
    }
    if (!windspeedPushStringLen(&buffer, &bufferRemaining, id, 2)) {
        return false;
    }
    if (!windspeedPushChar(&buffer, &bufferRemaining, ',')) {
        return false;
    }
    if (!windspeedPushString(&buffer, &bufferRemaining, payload)) {
        return false;
    }
    uint8_t checksum = windspeedComputeChecksum(windspeedTxBuffer.buffer + 1);
    windspeedEncodeChecksum(checksum, checksum_encoded);
    if (!windspeedPushChar(&buffer, &bufferRemaining, '*')) {
        return false;
    }
    if (!windspeedPushStringLen(&buffer, &bufferRemaining, checksum_encoded, 2)) {
        return false;
    }
    if (!windspeedPushString(&buffer, &bufferRemaining, "\r\n")) {
        return false;
    }
    if (!windspeedPushChar(&buffer, &bufferRemaining, '\0')) {
        return false;
    }
    if (bufferRemaining < 0) {
        return false;
    }
    return true;
}
