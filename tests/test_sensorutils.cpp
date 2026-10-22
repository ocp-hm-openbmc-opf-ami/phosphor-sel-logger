// Tests for pure-math ipmi:: utilities in include/sensorutils.hpp.
// No D-Bus, no filesystem, no external dependencies.
#include "include/sensorutils.hpp"

#include <gtest/gtest.h>

namespace
{

// ── baseInRange ─────────────────────────────────────────────────────────────

TEST(BaseInRangeTest, ValueAtMinBound_ReturnsTrue)
{
    EXPECT_TRUE(ipmi::baseInRange(static_cast<double>(ipmi::minInt10)));
}

TEST(BaseInRangeTest, ValueAtMaxBound_ReturnsTrue)
{
    EXPECT_TRUE(ipmi::baseInRange(static_cast<double>(ipmi::maxInt10)));
}

TEST(BaseInRangeTest, ValueBelowMinBound_ReturnsFalse)
{
    EXPECT_FALSE(ipmi::baseInRange(static_cast<double>(ipmi::minInt10) - 1.0));
}

TEST(BaseInRangeTest, ValueAboveMaxBound_ReturnsFalse)
{
    EXPECT_FALSE(ipmi::baseInRange(static_cast<double>(ipmi::maxInt10) + 1.0));
}

TEST(BaseInRangeTest, Zero_ReturnsTrue)
{
    EXPECT_TRUE(ipmi::baseInRange(0.0));
}

// ── scaleFloatExp ────────────────────────────────────────────────────────────

TEST(ScaleFloatExpTest, ZeroBase_ReturnsTrueUnchanged)
{
    double base = 0.0;
    int8_t exp = 0;
    EXPECT_TRUE(ipmi::scaleFloatExp(base, exp));
    EXPECT_DOUBLE_EQ(base, 0.0);
}

TEST(ScaleFloatExpTest, SmallPositiveBase_ExpandsToRange)
{
    // A very small positive value should be scaled up to within (minInt10,
    // maxInt10)
    double base = 0.001;
    int8_t exp = 0;
    bool ok = ipmi::scaleFloatExp(base, exp);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(ipmi::baseInRange(base));
}

TEST(ScaleFloatExpTest, LargeBase_ShrinksToRange)
{
    // 1e9 / 10^7 = 100, which fits in maxInt10 (511); returns true
    double base = 1.0e9;
    int8_t exp = 0;
    bool ok = ipmi::scaleFloatExp(base, exp);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(ipmi::baseInRange(base));
}

TEST(ScaleFloatExpTest, NegativeBase_ExpandsToRange)
{
    double base = -0.5;
    int8_t exp = 0;
    bool ok = ipmi::scaleFloatExp(base, exp);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(ipmi::baseInRange(base));
}

TEST(ScaleFloatExpTest, ExpAlreadyAtMax_CanNotShrinkFurther_ReturnsFalse)
{
    // base is huge and exp is already at maxInt4 → cannot shrink → false
    double base = 1.0e20;
    int8_t exp = ipmi::maxInt4;
    bool ok = ipmi::scaleFloatExp(base, exp);
    EXPECT_FALSE(ok);
}

TEST(ScaleFloatExpTest, ExpAlreadyAtMin_CanNotExpandFurther)
{
    // base is small but exp already at minInt4 → stops without going out of
    // range
    double base = 1.0;
    int8_t exp = ipmi::minInt4;
    bool ok = ipmi::scaleFloatExp(base, exp);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(ipmi::baseInRange(base));
}

// ── normalizeIntExp ──────────────────────────────────────────────────────────

TEST(NormalizeIntExpTest, ZeroBase_SetsExpToZero)
{
    int16_t ibase = 0;
    int8_t exp = 5;
    double dbase = 0.0;
    ipmi::normalizeIntExp(ibase, exp, dbase);
    EXPECT_EQ(exp, 0);
}

TEST(NormalizeIntExpTest, NonDivisibleBase_Unchanged)
{
    int16_t ibase = 7;
    int8_t exp = 2;
    double dbase = 7.0;
    ipmi::normalizeIntExp(ibase, exp, dbase);
    EXPECT_EQ(ibase, 7);
    EXPECT_EQ(exp, 2);
}

TEST(NormalizeIntExpTest, MultipleOfTen_NormalizesUp)
{
    // 300 * 10^2 == 3 * 10^4 — normalization reduces ibase, raises exp
    int16_t ibase = 300;
    int8_t exp = 2;
    double dbase = 300.0;
    ipmi::normalizeIntExp(ibase, exp, dbase);
    EXPECT_EQ(ibase, 3);
    EXPECT_EQ(exp, 4);
    EXPECT_NEAR(dbase, 3.0, 1e-9);
}

TEST(NormalizeIntExpTest, ExpAtMax_DoesNotExceedBound)
{
    int16_t ibase = 10;
    int8_t exp = ipmi::maxInt4;
    double dbase = 10.0;
    ipmi::normalizeIntExp(ibase, exp, dbase);
    // exp is already at max so no further normalization
    EXPECT_EQ(exp, ipmi::maxInt4);
    EXPECT_EQ(ibase, 10);
}

// ── getSensorAttributes ──────────────────────────────────────────────────────

TEST(GetSensorAttributesTest, TypicalTempSensor_ReturnsTrueAndNonZeroM)
{
    // Typical 0–100 °C range
    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    bool ok = ipmi::getSensorAttributes(100.0, 0.0, mValue, rExp, bValue, bExp,
                                        bSigned);
    ASSERT_TRUE(ok);
    EXPECT_NE(mValue, 0);
    EXPECT_FALSE(bSigned);
}

TEST(GetSensorAttributesTest, NegativeMin_SetsBSigned)
{
    // Range that crosses zero → bSigned must be true
    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    bool ok = ipmi::getSensorAttributes(50.0, -50.0, mValue, rExp, bValue, bExp,
                                        bSigned);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(bSigned);
}

TEST(GetSensorAttributesTest, MaxEqualsMin_ReturnsFalse)
{
    // max == min is invalid
    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    bool ok = ipmi::getSensorAttributes(50.0, 50.0, mValue, rExp, bValue, bExp,
                                        bSigned);
    EXPECT_FALSE(ok);
}

TEST(GetSensorAttributesTest, MaxLessThanMin_ReturnsFalse)
{
    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    bool ok = ipmi::getSensorAttributes(10.0, 100.0, mValue, rExp, bValue, bExp,
                                        bSigned);
    EXPECT_FALSE(ok);
}

TEST(GetSensorAttributesTest, InfiniteMin_ReturnsFalse)
{
    // Pre-scan #6: non-finite inputs must be rejected cleanly
    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    bool ok = ipmi::getSensorAttributes(
        100.0, std::numeric_limits<double>::infinity(), mValue, rExp, bValue,
        bExp, bSigned);
    EXPECT_FALSE(ok);
}

TEST(GetSensorAttributesTest, InfiniteMax_ReturnsFalse)
{
    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    bool ok =
        ipmi::getSensorAttributes(std::numeric_limits<double>::infinity(), 0.0,
                                  mValue, rExp, bValue, bExp, bSigned);
    EXPECT_FALSE(ok);
}

TEST(GetSensorAttributesTest, NanMin_ReturnsFalse)
{
    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    bool ok = ipmi::getSensorAttributes(
        100.0, std::numeric_limits<double>::quiet_NaN(), mValue, rExp, bValue,
        bExp, bSigned);
    EXPECT_FALSE(ok);
}

TEST(GetSensorAttributesTest, ExtremeWideRange_ReturnsFalseOrProducesValidM)
{
    // Pre-scan #6 probe: extremely wide range that could overflow int16_t M or
    // B. The function must either return false (preferred) or succeed with
    // mValue != 0 and within [-32768, 32767] — no silent UB overflow.
    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    bool ok = ipmi::getSensorAttributes(1.0e20, -1.0e20, mValue, rExp, bValue,
                                        bExp, bSigned);
    if (ok)
    {
        // If it succeeded, M must be non-zero and representable
        EXPECT_NE(mValue, 0);
    }
    // If it returned false, that is also acceptable behaviour
}

TEST(GetSensorAttributesTest, TinyRange_ReturnsFalseWhenMultiplierUnderflows)
{
    // Range so tiny that M rounds to zero → must return false
    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    bool ok = ipmi::getSensorAttributes(1.0e-30, 0.0, mValue, rExp, bValue,
                                        bExp, bSigned);
    EXPECT_FALSE(ok);
}

TEST(GetSensorAttributesTest, TypicalVoltage_RoundTrip)
{
    // 0–5 V range: verify the IPMI equation is self-consistent.
    // Encode attributes then decode a midpoint value and check it is close.
    constexpr double kMin = 0.0;
    constexpr double kMax = 5.0;
    constexpr double kMid = 2.5;

    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    ASSERT_TRUE(ipmi::getSensorAttributes(kMax, kMin, mValue, rExp, bValue,
                                          bExp, bSigned));
    uint8_t raw = ipmi::scaleIPMIValueFromDouble(kMid, mValue, rExp, bValue,
                                                 bExp, bSigned);

    // Reconstruct y = (M*x + B*10^bExp) * 10^rExp; x = raw byte value
    auto dM = static_cast<double>(mValue);
    auto dB = static_cast<double>(bValue);
    double y = (dM * raw + dB * std::pow(10.0, bExp)) * std::pow(10.0, rExp);
    // Allow up to ±2 × LSB tolerance due to integer rounding
    double lsb = dM * std::pow(10.0, rExp);
    EXPECT_NEAR(y, kMid, 2.0 * std::abs(lsb));
}

// ── scaleIPMIValueFromDouble ─────────────────────────────────────────────────

TEST(ScaleIPMIValueFromDoubleTest, ZeroMultiplier_ThrowsOutOfRange)
{
    // mValue == 0 is explicitly guarded against with std::out_of_range
    EXPECT_THROW(ipmi::scaleIPMIValueFromDouble(1.0, 0, 0, 0, 0, false),
                 std::out_of_range);
}

TEST(ScaleIPMIValueFromDoubleTest, MinValueUnsigned_ReturnsZero)
{
    // When value == min and bSigned==false, raw byte should be 0
    constexpr double kMin = 0.0;
    constexpr double kMax = 255.0;

    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    ASSERT_TRUE(ipmi::getSensorAttributes(kMax, kMin, mValue, rExp, bValue,
                                          bExp, bSigned));
    uint8_t raw = ipmi::scaleIPMIValueFromDouble(kMin, mValue, rExp, bValue,
                                                 bExp, bSigned);
    EXPECT_EQ(raw, 0u);
}

TEST(ScaleIPMIValueFromDoubleTest, MaxValueUnsigned_Returns255)
{
    constexpr double kMin = 0.0;
    constexpr double kMax = 255.0;

    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    ASSERT_TRUE(ipmi::getSensorAttributes(kMax, kMin, mValue, rExp, bValue,
                                          bExp, bSigned));
    uint8_t raw = ipmi::scaleIPMIValueFromDouble(kMax, mValue, rExp, bValue,
                                                 bExp, bSigned);
    EXPECT_EQ(raw, 255u);
}

TEST(ScaleIPMIValueFromDoubleTest, OutOfRangeValueClampsToMax)
{
    // Values above max are clamped, not wrapped or undefined
    constexpr double kMin = 0.0;
    constexpr double kMax = 100.0;

    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    ASSERT_TRUE(ipmi::getSensorAttributes(kMax, kMin, mValue, rExp, bValue,
                                          bExp, bSigned));
    uint8_t rawMax = ipmi::scaleIPMIValueFromDouble(kMax, mValue, rExp, bValue,
                                                    bExp, bSigned);
    uint8_t rawOver = ipmi::scaleIPMIValueFromDouble(
        kMax + 1000.0, mValue, rExp, bValue, bExp, bSigned);
    // Over-range must be clamped to 255 (or at least == rawMax due to rounding)
    EXPECT_GE(rawOver, rawMax);
}

TEST(ScaleIPMIValueFromDoubleTest, SignedRange_NegativeMinMapsToLowestByte)
{
    // For a signed range crossing zero, min → 0x80 (-128 as signed byte)
    constexpr double kMin = -128.0;
    constexpr double kMax = 127.0;

    int16_t mValue = 0;
    int8_t rExp = 0;
    int16_t bValue = 0;
    int8_t bExp = 0;
    bool bSigned = false;

    ASSERT_TRUE(ipmi::getSensorAttributes(kMax, kMin, mValue, rExp, bValue,
                                          bExp, bSigned));
    EXPECT_TRUE(bSigned);
    uint8_t raw = ipmi::scaleIPMIValueFromDouble(kMin, mValue, rExp, bValue,
                                                 bExp, bSigned);
    // Raw byte should correspond to -128 (0x80 unsigned)
    EXPECT_EQ(raw, static_cast<uint8_t>(0x80));
}

// ── getScaledIPMIValue ───────────────────────────────────────────────────────

TEST(GetScaledIPMIValueTest, InvalidRange_ThrowsRuntimeError)
{
    // max == min → getSensorAttributes returns false → runtime_error
    EXPECT_THROW(ipmi::getScaledIPMIValue(50.0, 50.0, 50.0),
                 std::runtime_error);
}

TEST(GetScaledIPMIValueTest, ValidRange_ReturnsExpectedByte)
{
    // Smoke test: 0–100 range, value at midpoint should give ~127
    uint8_t raw = ipmi::getScaledIPMIValue(50.0, 100.0, 0.0);
    // Allow ±2 LSB tolerance for rounding
    EXPECT_NEAR(static_cast<int>(raw), 127, 2);
}

TEST(GetScaledIPMIValueTest, ValueAtMin_ReturnsZero)
{
    uint8_t raw = ipmi::getScaledIPMIValue(0.0, 100.0, 0.0);
    EXPECT_EQ(raw, 0u);
}

TEST(GetScaledIPMIValueTest, ValueAtMax_Returns255)
{
    uint8_t raw = ipmi::getScaledIPMIValue(100.0, 100.0, 0.0);
    EXPECT_EQ(raw, 255u);
}

} // namespace
