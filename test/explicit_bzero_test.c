#include <assert.h>
#include <stddef.h>

#include "../util.h"

static void ExpectBufferZeroed(size_t len) {
  unsigned char buf[32];

  assert(len <= sizeof(buf));
  for (size_t i = 0; i < sizeof(buf); ++i) {
    buf[i] = (unsigned char)(i + 1);
  }

  explicit_bzero(buf, len);

  for (size_t i = 0; i < len; ++i) {
    assert(buf[i] == 0);
  }
  for (size_t i = len; i < sizeof(buf); ++i) {
    assert(buf[i] == (unsigned char)(i + 1));
  }
}

int main(void) {
  ExpectBufferZeroed(0);
  ExpectBufferZeroed(1);
  ExpectBufferZeroed(7);
  ExpectBufferZeroed(32);
  return 0;
}
