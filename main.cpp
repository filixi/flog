
#include "flog.h"

int main() {
  flog::FLog log;
  log(3, ' ', 4, ' ', 5, "\n");
  log(6, ' ', 7, ' ', 8, "\n");

  return 0;
}
