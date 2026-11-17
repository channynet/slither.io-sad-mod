#include "../app/src/network/skin_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t decode(const uint8_t* packed, size_t n, uint8_t* colors) {
  assert(n <= SKIN_WIRE_BYTES && !(n % 2) && n + 8 <= 255);
  size_t count = 0;
  for (size_t i = 0; i < n; i += 2) {
    assert(packed[i] > 0);
    for (int j = 0; j < packed[i]; j++) colors[count++] = packed[i + 1];
  }
  return count;
}
int main(void) {
  uint8_t input[256], original[256], packed[SKIN_WIRE_BYTES], output[256];
  assert(!skin_wire_encode(input, 0, packed));
  for (int length = 1; length <= 256; length++) {
    memset(input, 7, length);
    size_t n = skin_wire_encode(input, length, packed);
    assert(decode(packed, n, output) == (size_t)length);
    assert(!memcmp(input, output, length)); /* 256 identical colours must not wrap to zero. */
    for (int i = 0; i < length; i++) input[i] = (uint8_t)(i % 36);
    memcpy(original, input, length);
    n = skin_wire_encode(input, length, packed);
    assert(decode(packed, n, output) == (size_t)length);
    assert(!memcmp(input, original, length));
    if (length <= SKIN_WIRE_RUNS) assert(!memcmp(input, output, length));
    else {
      assert(output[0] == input[0]);
      for (int bucket = 0; bucket < SKIN_WIRE_RUNS; bucket++) {
        int start = bucket * length / SKIN_WIRE_RUNS;
        int end = (bucket + 1) * length / SKIN_WIRE_RUNS;
        for (int i = start; i < end; i++) assert(output[i] == input[(start + end - 1) / 2]);
      }
    }
  }
  puts("PASS skin wire: 1..256 segments, exact short/repeated designs, bounded long designs, original preserved");
}
