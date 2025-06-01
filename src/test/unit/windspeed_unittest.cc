#include <string>
#include <cstring>

extern "C" {
    #include "sensors/windspeed.h"
    #include "sensors/windspeed_unit.h"

    extern windspeedTxBuffer_t windspeedTxBuffer;
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

static windspeedParseError_t basicParse(const char *str) {
    std::string command(str);
    windspeedParsedResponse_t response;
    return windspeedParseResponse(&command[0], &response);
}

TEST(WINDSPEEDUnitTest, Checksum) {
    uint8_t checksum = windspeedComputeChecksum("02,DFP");
    EXPECT_EQ(checksum, 0x7C);
}

TEST(WINDSPEEDUnitTest, InvalidChecksumCharacter) {
    EXPECT_EQ(
        windspeedDecodeChecksum((windspeedEncodedChecksum_t){'7', 'P'}),
        WINDSPEED_INVALID_CHECKSUM
    );
    EXPECT_EQ(
        windspeedDecodeChecksum((windspeedEncodedChecksum_t){'U', 'C'}),
        WINDSPEED_INVALID_CHECKSUM
    );
    EXPECT_EQ(
        windspeedDecodeChecksum((windspeedEncodedChecksum_t){'O', 'L'}),
        WINDSPEED_INVALID_CHECKSUM
    );
}

TEST(WINDSPEEDUnitTest, ChecksumDecoding) {
    EXPECT_EQ(
        windspeedDecodeChecksum((windspeedEncodedChecksum_t){'7', 'C'}),
        0x7C
    );
    EXPECT_EQ(
        windspeedDecodeChecksum((windspeedEncodedChecksum_t){'0', '0'}),
        0x00
    );
    EXPECT_EQ(
        windspeedDecodeChecksum((windspeedEncodedChecksum_t){'F', 'F'}),
        0xFF
    );
}

TEST(WINDSPEEDUnitTest, ParseNoDollar) {
    EXPECT_EQ(basicParse("02,DFP*7C\r\n"), windspeedParseNoDollar);
}

TEST(WINDSPEEDUnitTest, ParseNoComma) {
    EXPECT_EQ(basicParse("$02DFP*7C\r\n"), windspeedParseNoComma);
}

TEST(WINDSPEEDUnitTest, ParseNoAsterisk) {
    EXPECT_EQ(basicParse("$02,DFP7C\r\n"), windspeedParseNoAsterisk);
}

TEST(WINDSPEEDUnitTest, ParseNoCRLF) {
    EXPECT_EQ(basicParse("$02,DFP*7C\n"), windspeedParseNoCRLF);
}

TEST(WINDSPEEDUnitTest, ParseBadIDLength) {
    EXPECT_EQ(basicParse("$,DFP*7C\r\n"), windspeedParseBadIDLength);
    EXPECT_EQ(basicParse("$2,DFP*7C\r\n"), windspeedParseBadIDLength);
    EXPECT_EQ(basicParse("$002,DFP*7C\r\n"), windspeedParseBadIDLength);
}

TEST(WINDSPEEDUnitTest, ParseBadChecksumLength) {
    EXPECT_EQ(basicParse("$02,DFP*\r\n"), windspeedParseBadChecksumLength);
    EXPECT_EQ(basicParse("$02,DFP*C\r\n"), windspeedParseBadChecksumLength);
    EXPECT_EQ(basicParse("$02,DFP*17C\r\n"), windspeedParseBadChecksumLength);
}

TEST(WINDSPEEDUnitTest, ParseOk) {
    std::string command("$02,DFP*7C\r\n");
    windspeedParsedResponse_t response;
    windspeedParseError_t ret = windspeedParseResponse(&command[0], &response);
    EXPECT_EQ(ret, windspeedParseOk);
    EXPECT_STREQ(response.payload_start, "DFP");
    ASSERT_TRUE(std::memcmp(response.id, "02", sizeof(response.id)) == 0)
        << "Bad ID: " << response.id[0] << response.id[1];
    ASSERT_TRUE(std::memcmp(response.checksum, "7C", sizeof(response.checksum)) == 0)
        << "Bad checksum: " << response.checksum[0] << response.checksum[1];
}

TEST(WINDSPEEDUnitTest, BasicEncode) {
    windspeedEncodedID_t id{'0', '2'};
    bool success = windspeedPrepareCommand(id, "DFP");
    EXPECT_TRUE(success);
    EXPECT_STREQ(windspeedTxBuffer.buffer, "$02,DFP*7C\r\n");
}

// $ - 1 char
// id - 2 chars
// , - 1 char
// payload - n chars
// * - 1 char
// checksum - 2 chars
// crlf - 2 chars
// null terminator - 1 char
const size_t MAX_PAYLOAD_LENGTH = WINDSPEED_TX_BUFFER_SIZE - (1 + 2 + 1 + 1 + 2 + 2 + 1);

TEST(WINDSPEEDUnitTest, EncodeWithMaxLength) {
    windspeedEncodedID_t id{'0', '2'};
    std::string command = std::string(MAX_PAYLOAD_LENGTH, 'A');
    bool success = windspeedPrepareCommand(id, command.c_str());
    EXPECT_TRUE(success);
    std::string expected_result = "$02," + command + "*2E\r\n";
    EXPECT_STREQ(windspeedTxBuffer.buffer, expected_result.c_str());
}

TEST(WINDSPEEDUnitTest, EncodeTooLarge) {
    windspeedEncodedID_t id{'0', '2'};
    std::string command = std::string(MAX_PAYLOAD_LENGTH+1, 'A');
    bool success = windspeedPrepareCommand(id, command.c_str());
    EXPECT_FALSE(success);
}
