
#include <thread>

#include "flog.h"

int main() {
  flog::FLog log;
  log(1, 2, 3, '\n');
  return 0;
}
