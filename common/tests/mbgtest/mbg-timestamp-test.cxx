#include "frontend_utils.hh"

int main(int argc, char* argv[])
{
  std::vector<std::string> outvec;
 
  for (int i = 0; i < 20; ++i) {
    outvec.push_back(exec("mbgfasttstamp"));
  }

  for (auto s : outvec) {
    std::cout << s << std::endl;
    std::cout << parse_mbg_string_ns(s) * 1.0e-9 << std::endl;
  }

  return 0;
}
