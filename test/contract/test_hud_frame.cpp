// Standard library
#include <cstring>

// Project
#include "contract/hud_frame.h"
#include "test_harness.h"

namespace
{

int test_seqlock_roundtrip()
{
    OrangeHudFrame mapped{};
    OrangeHudFrame in{};
    in.enabled = 1;
    in.config_gen = 7;
    in.show[orange::kOverlayMetricFps] = 1;
    std::strcpy(in.value[orange::kOverlayMetricFps][0], "59.9");

    orange_hud_seqlock_store(&mapped, &in);
    TEST_ASSERT_EQ_INT(0, static_cast<int>(mapped.seq & 1u));

    OrangeHudFrame out{};
    TEST_ASSERT_EQ_INT(0, orange_hud_seqlock_load(&mapped, &out));
    TEST_ASSERT_EQ_INT(static_cast<int>(ORANGE_HUD_MAGIC), static_cast<int>(out.magic));
    TEST_ASSERT_EQ_INT(7, static_cast<int>(out.config_gen));
    TEST_ASSERT_TRUE(std::strcmp(out.value[orange::kOverlayMetricFps][0], "59.9") == 0);
    return 0;
}

int test_odd_sequence_is_rejected()
{
    OrangeHudFrame mapped{};
    mapped.seq = 3; // writer mid-update
    OrangeHudFrame out{};
    TEST_ASSERT_TRUE(orange_hud_seqlock_load(&mapped, &out) != 0);
    return 0;
}

/*
 * The HUD frame is shared with the module that runs inside SceShellUI, which
 * can outlive the daemon. If this fails, the layout changed: update the
 * numbers here AND bump ORANGE_HUD_ABI in contract/hud_frame.h.
 */
int test_frame_layout_is_the_abi()
{
    TEST_ASSERT_EQ_INT(0, static_cast<int>(offsetof(OrangeHudFrame, magic)));
    TEST_ASSERT_EQ_INT(4, static_cast<int>(offsetof(OrangeHudFrame, seq)));
    TEST_ASSERT_EQ_INT(8, static_cast<int>(offsetof(OrangeHudFrame, unix_ns)));
    TEST_ASSERT_EQ_INT(16, static_cast<int>(offsetof(OrangeHudFrame, config_gen)));
    TEST_ASSERT_EQ_INT(20, static_cast<int>(offsetof(OrangeHudFrame, enabled)));
    TEST_ASSERT_EQ_INT(21, static_cast<int>(offsetof(OrangeHudFrame, background)));
    TEST_ASSERT_EQ_INT(22, static_cast<int>(offsetof(OrangeHudFrame, pos)));
    TEST_ASSERT_EQ_INT(23, static_cast<int>(offsetof(OrangeHudFrame, align)));
    TEST_ASSERT_EQ_INT(24, static_cast<int>(offsetof(OrangeHudFrame, font_size)));
    TEST_ASSERT_EQ_INT(25, static_cast<int>(offsetof(OrangeHudFrame, per_core)));
    TEST_ASSERT_EQ_INT(26, static_cast<int>(offsetof(OrangeHudFrame, show)));
    TEST_ASSERT_EQ_INT(30, static_cast<int>(offsetof(OrangeHudFrame, order)));
    TEST_ASSERT_EQ_INT(34, static_cast<int>(offsetof(OrangeHudFrame, value)));
    TEST_ASSERT_EQ_INT(616, static_cast<int>(sizeof(OrangeHudFrame)));
    TEST_ASSERT_EQ_INT(640, ORANGE_HUD_FRAME_BYTES);
    TEST_ASSERT_EQ_INT(48, ORANGE_HUD_TEXT_LEN);
    TEST_ASSERT_EQ_INT(0x4F485544, static_cast<int>(ORANGE_HUD_MAGIC));
    return 0;
}

int test_frame_fits_record()
{
    TEST_ASSERT_TRUE(sizeof(OrangeHudFrame) <= ORANGE_HUD_FRAME_BYTES);
    return 0;
}

} // namespace

extern "C" int test_hud_frame_suite(void)
{
    int failures = 0;
    failures += test_run("hud_frame_roundtrip", test_seqlock_roundtrip);
    failures += test_run("hud_frame_odd_seq", test_odd_sequence_is_rejected);
    failures += test_run("hud_frame_size", test_frame_fits_record);
    failures += test_run("hud_frame_layout_abi", test_frame_layout_is_the_abi);
    return failures;
}
