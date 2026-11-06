/* Host regression tests for atomic x86-64 entry-patch construction. */
// Standard library
#include <cstdint>
#include <cstring>

// Project
#include "detour/hotpatch.h"
#include "test_harness.h"

namespace
{

uint64_t read_u64(const uint8_t* p)
{
    uint64_t value = 0;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

int test_builds_absolute_jump_and_preserves_tail()
{
    uint8_t current[ORANGE_X64_ATOMIC_PATCH_SIZE];
    for (size_t i = 0; i < sizeof(current); ++i)
    {
        current[i] = static_cast<uint8_t>(0xa0 + i);
    }

    x64_atomic_patch patch{};
    constexpr uintptr_t target = 0x1000;
    constexpr uintptr_t hook = 0x123456789abcdef0ULL;
    TEST_ASSERT_TRUE(x64_build_atomic_patch(target, current, hook, &patch));

    TEST_ASSERT_MEMEQ(current, patch.expected, sizeof(current));
    const uint8_t prefix[] = {0xff, 0x25, 0, 0, 0, 0};
    TEST_ASSERT_MEMEQ(prefix, patch.desired, sizeof(prefix));
    TEST_ASSERT_EQ_U64(hook, read_u64(patch.desired + sizeof(prefix)));
    TEST_ASSERT_EQ_INT(current[14], patch.desired[14]);
    TEST_ASSERT_EQ_INT(current[15], patch.desired[15]);
    return 0;
}

int test_rejects_unaligned_target()
{
    uint8_t current[ORANGE_X64_ATOMIC_PATCH_SIZE] = {};
    x64_atomic_patch patch{};
    TEST_ASSERT_TRUE(!x64_build_atomic_patch(0x1008, current, 0x2000, &patch));
    return 0;
}

int test_rejects_invalid_arguments()
{
    uint8_t current[ORANGE_X64_ATOMIC_PATCH_SIZE] = {};
    x64_atomic_patch patch{};
    TEST_ASSERT_TRUE(!x64_build_atomic_patch(0x1000, nullptr, 0x2000, &patch));
    TEST_ASSERT_TRUE(!x64_build_atomic_patch(0x1000, current, 0, &patch));
    TEST_ASSERT_TRUE(!x64_build_atomic_patch(0x1000, current, 0x2000, nullptr));
    return 0;
}

} // namespace

extern "C" int test_hotpatch_suite(void)
{
    int failures = 0;
    failures += test_run("hotpatch.build", test_builds_absolute_jump_and_preserves_tail);
    failures += test_run("hotpatch.alignment", test_rejects_unaligned_target);
    failures += test_run("hotpatch.arguments", test_rejects_invalid_arguments);
    return failures;
}
