#pragma once

#if defined(PLATFORM_APPLE) && defined(SUPPORT_UPSCALING)
bool isVideoUpscalingSupported();
#else
inline bool isVideoUpscalingSupported() { return true; }
#endif

