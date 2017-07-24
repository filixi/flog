
#include <thread>

#include "flog.h"

int main() {
  flog::LogSplit(flog::CurrentTick, 1, 2, 3) << std::endl;
  return 0;
}
