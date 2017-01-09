#include <cstdio>
#include <iostream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

std::string exec(const char *cmd) 
{
  char buffer[128];
  std::string result("");
  std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
  
  if (!pipe) throw std::runtime_error("popen() failed!");

  while (!feof(pipe.get())) {
    if (fgets(buffer, 128, pipe.get()) != NULL) {
      result += buffer;
    }
  }
  
  return result;
}

uint64_t parse_mbg_string_us(std::string ts) 
{
  std::stringstream ss;
  std::string timestamp;
  std::string str;
  ss.str(ts);

  bool valid_ts = false;

  while (!ss.good()) {
    std::getline(ss, str);

    if (str.find("HR time raw:") == 0) {
      valid_ts = true;
      timestamp = str;
      break;
    }
  }

  if (valid_ts) {
    
    ss.str(timestamp);

    std::getline(ss, str, ':');
    std::cout << str << std::endl;

    std::getline(ss, str, ',');
    std::cout << str << std::endl;

    std::getline(ss, str, ',');
    std::cout << str << std::endl;
  } else {
    std::cout << "No valid timestamp found." << std::endl;
  }

  return 0;
}


int main(int argc, char* argv[])
{
  std::vector<std::string> outvec;
 
  for (int i = 0; i < 20; ++i) {
    outvec.push_back(exec("mbgfasttstamp"));
  }

  for (auto s : outvec) {
    //    std::cout << s << std::endl;
    parse_mbg_string_us(s);
  }

  return 0;
}
