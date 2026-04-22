#include <stdlib.h>
#include <string.h>

#include "../helpers/authproto.h"

int main(void) {
  int eof_permitted = 0;
  for (;;) {
    char type;
    char *message;
    type = ReadPacket(0, &message, eof_permitted);
    if (type == 0) {
      return 0;
    }
    if (!WritePacketBytes(1, type, message, strlen(message))) {
      free(message);
      return 1;
    }
    free(message);
    eof_permitted = !eof_permitted;
  }
}
