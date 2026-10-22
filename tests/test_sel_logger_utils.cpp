// Tests for toHexStr() and isValidSensorType() from include/sel_logger.hpp.
// Include intel-ipmi-oem headers first to provide inline implementations of
// getSensorNumberFromPath() et al. which doPefTask() (also in the header)
// references — mirroring the include order in src/sel_logger.cpp.
#include <intel-ipmi-oem/sdrutils.hpp>
#include <sel_logger.hpp>

#include <iomanip>
#include <sstream>
#include <vector>

#ifdef FAIL
#undef FAIL
#endif
#ifdef ERROR
#undef ERROR
#endif

#include <gtest/gtest.h>

namespace
{

// ── toHexStr ─────────────────────────────────────────────────────────────────

TEST(ToHexStrTest, EmptyVector_ProducesEmptyString)
{
    std::string result;
    toHexStr({}, result);
    EXPECT_TRUE(result.empty());
}

TEST(ToHexStrTest, SingleByteZero_ProducesDoubleZero)
{
    std::string result;
    toHexStr({0x00}, result);
    EXPECT_EQ(result, "00");
}

TEST(ToHexStrTest, SingleByteFf_ProducesFF)
{
    std::string result;
    toHexStr({0xFF}, result);
    EXPECT_EQ(result, "FF");
}

TEST(ToHexStrTest, MultipleBytes_ProducesUppercaseHex)
{
    // 0xDE, 0xAD, 0xBE, 0xEF → "DEADBEEF"
    std::string result;
    toHexStr({0xDE, 0xAD, 0xBE, 0xEF}, result);
    EXPECT_EQ(result, "DEADBEEF");
}

TEST(ToHexStrTest, LowNibbleValues_ZeroPadded)
{
    // Each byte below 0x10 must produce two characters (zero-padded)
    std::string result;
    toHexStr({0x01, 0x0A, 0x0F}, result);
    EXPECT_EQ(result, "010A0F");
}

TEST(ToHexStrTest, AllBytes_CorrectLength)
{
    // A 3-byte vector must produce a 6-character string
    std::string result;
    toHexStr({0x12, 0x34, 0x56}, result);
    EXPECT_EQ(result.size(), 6u);
}

TEST(ToHexStrTest, OutputIsUppercase)
{
    std::string result;
    toHexStr({0xab, 0xcd, 0xef}, result);
    EXPECT_EQ(result, "ABCDEF");
}

TEST(ToHexStrTest, OverwritesPreviousContent)
{
    std::string result = "garbage";
    toHexStr({0x01}, result);
    EXPECT_EQ(result, "01");
}

// ── isValidSensorType
// ─────────────────────────────────────────────────────────

TEST(IsValidSensorTypeTest, ZeroReserved_ReturnsFalse)
{
    // 0x00 is reserved by the IPMI spec
    EXPECT_FALSE(isValidSensorType(0x00));
}

TEST(IsValidSensorTypeTest, FirstStandardType_ReturnsTrue)
{
    EXPECT_TRUE(isValidSensorType(0x01));
}

TEST(IsValidSensorTypeTest, LastStandardType_ReturnsTrue)
{
    EXPECT_TRUE(isValidSensorType(0x2C));
}

TEST(IsValidSensorTypeTest, FirstReservedGap_ReturnsFalse)
{
    // 0x2D–0xBF are reserved/out-of-range per spec
    EXPECT_FALSE(isValidSensorType(0x2D));
}

TEST(IsValidSensorTypeTest, MiddleOfReservedGap_ReturnsFalse)
{
    EXPECT_FALSE(isValidSensorType(0x80));
}

TEST(IsValidSensorTypeTest, LastReservedGap_ReturnsFalse)
{
    EXPECT_FALSE(isValidSensorType(0xBF));
}

TEST(IsValidSensorTypeTest, FirstOemType_ReturnsTrue)
{
    // 0xC0–0xFF are OEM-defined and valid
    EXPECT_TRUE(isValidSensorType(0xC0));
}

TEST(IsValidSensorTypeTest, LastOemType_ReturnsTrue)
{
    EXPECT_TRUE(isValidSensorType(0xFF));
}

TEST(IsValidSensorTypeTest, MidStandardRange_ReturnsTrue)
{
    EXPECT_TRUE(isValidSensorType(0x10));
}

TEST(IsValidSensorTypeTest, MidOemRange_ReturnsTrue)
{
    EXPECT_TRUE(isValidSensorType(0xDF));
}

} // namespace
