#include "FFmpegVideoDecoderPlatformHelpers.hpp"

namespace ffmpeg::decoder {

#if defined(USE_METAL_RENDERER)
bool useMetalDirectHardwareFrames() {
    return true;
}
#endif

} // namespace ffmpeg::decoder