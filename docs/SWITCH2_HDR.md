# Switch 2 HDR implementation assessment

Checked on 2026-11-14 against the installed SDK:

- ounce-sdk 20261113
- libounce db3f88a3
- OunceVK 791c34e

True HDR output is blocked by the installed display driver. This assessment
does not enable an HDR setting or change the installed application.

Update: public SDK binaries and sources were subsequently located under
[`ouncebrew/ounce-devkit`](https://github.com/ouncebrew/ounce-devkit),
[`ouncebrew/ouncevk`](https://github.com/ouncebrew/ouncevk), and
[`ouncebrew/libounce`](https://github.com/ouncebrew/libounce). The source-availability
limitation below describes the original `kurumuz/libounce` research export, not
the newly published repositories. HDR presentation still requires implementation
and qualification; this assessment is specifically about the installed 20261113
SDK and does not establish the capabilities of a newer release.

## Verified SDK boundary

Ordinary GNU `nm`/`objdump` static inspection of the SDK's
`portlibs/switch2/lib/libouncevk.a` establishes:

- `native_formats` and `native_formats2` return one surface format. Its color
  space is always zero (`VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`). The pixel format
  is `VK_FORMAT_R8G8B8A8_UNORM` (37), or the 8-bit sRGB variant (43) on another
  backend path.
- The native-window path in `native_create_chain`, at offsets 0x2b4-0x2c4,
  requires format 37 and rejects a nonzero color space. The other path also
  requires an 8-bit format and color space zero. Choosing an HDR10 color space
  or a 10-bit format in Moonlight would fail this validation.
- `native_vi_import` currently registers a fixed native scanout image format.
  Advertising additional Vulkan surface formats alone would not implement the
  required image registration or display signaling.

This is static SDK evidence, not a live console HDR capability test. REA tools
were unavailable; the observations above are from ordinary shell tools.
Extracted objects/disassembly and input hashes are retained locally under
`build/switch2-hdr-inspect/`.

The installed libounce headers expose `Vi2DisplayModeCapability.hdr_output_formats`
and `vi2QueryDisplayModeCapability`, but no verified HDR output-setting API.
`Vi2PresentOptions.metadata` is explicitly documented as active service input
with fields of unverified meaning. Its layout must be established before use.

The native decoder service exposes `ViddecOutputFormat_RgbaHdr` for AV1/VP9.
However, `videoDecoderCreate`, `VideoFrame` and `videoDecoderCopyFrame` expose an
8-bit NV12 helper contract. Moonlight currently rejects AV1 Main10 on its native
path and its Vulkan shader reads 8-bit NV12/YUV420P. The HDR decoder enum alone
does not establish the returned pixel layout, transfer function or display HDR.

The public [libounce graphics documentation](https://github.com/kurumuz/libounce/blob/202f647387d47634c23498e0f0743791cbd61989/docs/experimental-graphics.md)
states that the private device/queue implementation is omitted from the public
research export. That checkout does not provide the installed OunceVK source.

## Work required for real HDR

1. Extend the OunceVK/VI2 presentation implementation using verified native
   image formats and HDR signaling. Advertise an HDR-capable Vulkan surface
   format/color-space pair only when the active handheld/TV output supports it.
   Support mastering metadata and restore SDR cleanly on stream exit and
   dock/output changes.
2. Extend native AV1 Main10 decoding with an established 10-bit/HDR image layout
   and color policy. Preserve ownership and synchronization through GPU use.
   HEVC Main10 would currently need software decoding because the SDK has no
   HEVC codec entry; H.264 is not a Moonlight HDR transport.
3. Add 10-bit pixel handling and BT.2020/PQ conversion in Moonlight's Vulkan
   renderer. Composite SDR UI at an explicit reference white through a correctly
   managed HDR path rather than treating SDR bytes as PQ values.
4. Gate HDR negotiation and settings on the complete decode/presentation path.
   Pass host HDR state and mastering metadata through the session. Handle
   unsupported outputs explicitly, with SDR fallback or a separately identified
   tone-mapped mode.
5. Test known black/reference-white/highlight patterns, gradients, HDR metadata,
   dock/undock, HDR-disabled TVs, stream reconnect and return to SDR. Host
   compilation or an enabled checkbox is insufficient qualification.

The next step is source-level investigation of the published OunceVK/libounce
implementation and development of the HDR boundary described above, followed by
console qualification. Public source availability supersedes the original
request for the private SDK tree; it does not itself enable HDR presentation.
