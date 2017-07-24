
#include <fstream>
#include <limits>
#include <thread>

#include "flog.h"

class A {};

void foo() {
  flog::LogSplit(flog::CurrentTick, 1, 2, 3) << std::endl;
}

int main() {
  flog::LogSplit(flog::AscTime, 1, 2, 3) << std::endl;
  std::thread(foo).join();
  return 0;
}
