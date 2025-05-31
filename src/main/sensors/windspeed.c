#include "windspeed.h"
#include "windspeed_unit.h"

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
    (void)parsed;
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
