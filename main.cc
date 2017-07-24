
#include <fstream>
#include <limits>
#include <thread>

#include "flog.h"

class A {};

int main() {
  flog::LogSplit(flog::AscTime, 1, 2, 3) << std::endl;
  flog::LogSplit(flog::CurrentTick, 1, 2, 3) << std::endl;
  return 0;
}
