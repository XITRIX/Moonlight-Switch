# GameShare research branch

The standalone investigation is in [research/gameshare-probe](../research/gameshare-probe/START_HERE.md).
That start page includes another-PC setup, pairing with the existing probe,
the original runtime profile, standalone build instructions and the evidence index.

Native StreamPlay initialization, work buffers, guest display/layer, guest HID
memory and the normal communication check succeed on firmware 23.0.1.
Matchmaking preparation still returns 0x2fab1f (287/6101); its exact condition
is unknown. There is no working host advertisement, guest connection or image
stream. This branch does not add GameShare hosting to Moonlight itself.

The intended goal is PC -> Moonlight on Switch 2 -> native GameShare guest,
with guest input returned to the PC as player 2. HEN and RO must remain
unchanged. See [the latest results](../research/gameshare-probe/HOST_BRINGUP.md).
