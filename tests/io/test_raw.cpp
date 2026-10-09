#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "compositor/io/image_codec.hpp"

namespace compositor::io {
namespace {

TEST(Raw, RecognizesRawExtensions) {
    EXPECT_TRUE(static_cast<bool>(is_raw_extension("cr2")));
    EXPECT_TRUE(static_cast<bool>(is_raw_extension(".NEF")));
    EXPECT_TRUE(static_cast<bool>(is_raw_extension("DNG")));
    EXPECT_FALSE(static_cast<bool>(is_raw_extension("png")));
    EXPECT_FALSE(static_cast<bool>(is_raw_extension("")));
}

TEST(Raw, RejectsGarbage) {
    const std::vector<std::uint8_t> garbage = {'n', 'o', 't', 'r', 'a', 'w'};
    EXPECT_THROW((void)decode_raw(garbage), ImageCodecError);
}

}  // namespace
}  // namespace compositor::io
