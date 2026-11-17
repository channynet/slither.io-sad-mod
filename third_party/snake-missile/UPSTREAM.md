# Snake Missile integration

Source: https://github.com/ShimadaNanaki/snake-missile
Upstream revision: ee8027215f7860f1b2fe606dfacb68b1bf65cf04
License: MIT; see LICENSE.

The native-worker entry point uses the original Earthworm connection, proxy,
handshake, spawn and movement implementation for legacy servers. Native integration
adds cancellation, reconnect backoff, bounded connection counts, packet validation
and a live target supplied by SlitherNative instead of (32000, 32000).
The current raw/container protocol adapter in application/native_packets.go is
adapted from the host's GPL network parser. The native actor loop owns each worm's
state and reconnects after death. Proxy URL parsing accepts standard URI syntax.
