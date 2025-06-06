#include "windspeed.h"
#include "drivers/serial.h"
#include "io/serial.h"
#include "windspeed_unit.h"
#include <string.h>
#include <stdlib.h>

STATIC_UNIT_TESTED windspeedBuffer_t windspeedTxBuffer;
static windspeedBuffer_t windspeedRxBuffer;

static serialPort_t *port = NULL;

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

void windspeedInit(void) {
    const serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_WINDSPEED);
    if (!portConfig) {
        return;
    }

    port = openSerialPort(
        portConfig->identifier,
        FUNCTION_WINDSPEED,
        NULL, NULL,
        4800, // TODO: make configurable
        MODE_RXTX,
        SERIAL_STOPBITS_1 | SERIAL_PARITY_NO
    );
}

static bool windspeedTryPushingRx(char character) {
    // make sure nullbyte fits
    if (windspeedRxBuffer.buffer_filled >= WINDSPEED_TX_BUFFER_SIZE - 1) {
        return false;
    }
    windspeedRxBuffer.buffer[windspeedRxBuffer.buffer_filled++] = character;
    windspeedRxBuffer.buffer[windspeedRxBuffer.buffer_filled] = 0;
    return true;
}

typedef enum {
    windspeedStateInitial = 0,
    windspeedStateWaitingForId,
    windspeedStateWaitingForPeriod,
    windspeedStateRunning,
    windspeedStateErrored,
} windspeedState_t;

// static const windspeedEncodedID_t ID = {'0', '1'};

static char DATA_RECEIVED[32];

char *windspeedGetLine(void) {
    return DATA_RECEIVED;
}

static windspeedState_t state = windspeedStateInitial;
void windspeedUpdate(timeUs_t currentTimeUs) {
    (void)currentTimeUs;
    (void)windspeedPrepareCommand;
    (void)windspeedParseResponse;
    (void)windspeedDecodeChecksum;
    // bool transmitCommand = false;

    if (port == NULL || state == windspeedStateErrored) {
        return;
    }

    if (!isSerialTransmitBufferEmpty(port)) {
        return;
    }

    for (uint32_t i = 0; i < serialRxBytesWaiting(port); i++) {
        if (!windspeedTryPushingRx(serialRead(port))) {
            windspeedRxBuffer.buffer_filled = 0;
            return;
        }
    }

    windspeedParsedResponse_t parsed;
    bool haveRx = windspeedParseResponse(windspeedRxBuffer.buffer, &parsed) == windspeedParseOk;
    if (haveRx) {
        windspeedRxBuffer.buffer_filled = 0;
        unsigned int charsToCopy = strlen(parsed.payload_start);
        if (charsToCopy > sizeof(DATA_RECEIVED) - 1) {
            charsToCopy = sizeof(DATA_RECEIVED) - 1;
        }
        memset(DATA_RECEIVED, 0, sizeof(DATA_RECEIVED));
        memcpy(DATA_RECEIVED, parsed.payload_start, charsToCopy);
    }

    // switch (state) {
    //     case windspeedStateInitial: {
    //         if (!windspeedPrepareCommand(ID, "ID?")) {
    //             state = windspeedStateErrored;
    //             return;
    //         }
    //         transmitCommand = true;
    //         state = windspeedStateWaitingForId;
    //     }; break;
    //     case windspeedStateWaitingForId: {
    //         if (!haveRx) break;
    //         if (strncmp(parsed.payload_start, "ID=", 3) == 0) {
    //             if (strlen(parsed.payload_start) > sizeof(ID_RECEIVED) + 1) {
    //                 state = windspeedStateErrored;
    //                 return;
    //             }
    //             if (!windspeedPrepareCommand(ID, "CU?")) {
    //                 state = windspeedStateErrored;
    //                 return;
    //             }
    //             transmitCommand = true;
    //             state = windspeedStateWaitingForPeriod;
    //         }
    //     }; break;
    //     case windspeedStateWaitingForPeriod: {
    //         if (!haveRx) break;
    //         if (strncmp(parsed.payload_start, "CU=", 3) == 0) {
    //             if (strlen(parsed.payload_start) > sizeof(ID_RECEIVED) + 1) {
    //                 state = windspeedStateErrored;
    //                 return;
    //             }
    //             int payloadLen = strlen(parsed.payload_start);
    //             char enabled = parsed.payload_start[3] == 'E';
    //             if (payloadLen < 6) {
    //
    //             }
    //             int timing = atoi(parsed.payload_start + 5);
    //             (void)timing;
    //             (void)enabled;
    //             // transmitCommand = true;
    //             state = windspeedStateWaitingForPeriod;
    //         }
    //     }; break;
    //     case windspeedStateRunning: {
    //         if (!haveRx) break;
    //
    //     }; break;
    //     case windspeedStateErrored: {
    //
    //     }; break;
    // }

    // if (transmitCommand) {
    //     serialWriteBuf(port, (uint8_t*)windspeedTxBuffer.buffer, strlen(windspeedTxBuffer.buffer));
    // }
}
