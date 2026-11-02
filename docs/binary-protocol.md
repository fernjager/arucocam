# Binary Transport (Reserved)

**Status: not implemented.**  The JSON Lines protocol is the stable interface.
This page records a planned layout for a future fixed-size binary transport; do
not write a parser against it yet.

The layout is little-endian:

```text
Header (16 bytes)
  uint32 magic      0x4D414341  ("ACAM")
  uint16 version    1
  uint16 header_len 16
  uint32 marker_len 16
  uint32 count

Marker (16 bytes)
  uint16 id
  int16  bearing_cdeg     degrees * 100, positive right
  uint16 horiz_cm         planar horizontal distance, centimetres
  int16  vert_cm          camera-relative vertical offset, centimetres
  uint32 reserved
  uint32 reserved2
```

Notes for a future implementation:

- Records are packed in the order shown; there is no implicit compiler padding.
  The two 32-bit reserved fields make each marker record exactly 16 bytes.
- `version` allows the header and marker records to evolve.
- `header_len` and `marker_len` make it possible to skip unknown trailing
  fields.
- `count` is the number of marker records immediately following the header.
- Distances are centimetres and bearings are centidegrees, which matches the
  precision of the JSON output without floating-point encoding.
- The current JSON field names, units, and sign convention are the contract to
  preserve.
