#include <iostream>

extern "C" void slisp_init() {
  std::cout << "init!" << std::endl;
}
