#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include "utility.h"
#include <iostream>
#include <fstream>
#if defined(_WIN32) || defined(_WIN64)
#include <winsock.h>
#elif defined(__APPLE__) || defined(__MACH__)
#include <pwd.h>
#include <uuid/uuid.h>
#elif defined(__unix) || defined(__unix__)
#include <pwd.h>
#include <arpa/inet.h>
#endif
#include <thread>
#include <ctime>


namespace Zigurat
{

  std::string Utility::os_name()
  {
#if defined(_WIN32) || defined(_WIN64)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#elif defined(__APPLE__) || defined(__MACH__)
    return "Darwin";
#elif defined(sun) || defined(__sun)
    return "SunOS";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#elif defined(__unix) || defined(__unix__)
    return "Unix";
#else
    return "Unknown";
#endif
  }

  std::string Utility::user_home()
  {
    const char* home_dir;
#if defined(_WIN32) || defined(_WIN64)
    home_dir = getenv("HOMEPATH");
#else
    if ((home_dir = getenv("HOME")) == NULL) {
      home_dir = getpwuid(getuid())->pw_dir;
    }
#endif
    return std::string(home_dir, strlen(home_dir));
  }

  std::string Utility::env_var(std::string name)
  {
    const char* value;
    if ((value = getenv(name.c_str())) == NULL) {
      return "";
    }
    return std::string(value, strlen(value));
  }

  bool Utility::in(char ch, const std::string string)
  {
    return (string.find(ch) != std::string::npos);
  }

  std::string Utility::trim_left(const std::string& str, char chr)
  {
    size_t first = str.find_first_not_of(chr);
    if (first == std::string::npos)
      return str;
    return str.substr(first, (str.size() - first + 1));
  }

  std::string Utility::trim_right(const std::string& str, char chr)
  {
    size_t last = str.find_last_not_of(chr);
    return str.substr(0, (last + 1));
  }

  std::string Utility::trim(const std::string& str, char chr)
  {
    size_t first = str.find_first_not_of(chr);
    if (first == std::string::npos)
      return "";
    size_t last = str.find_last_not_of(chr);
    return str.substr(first, (last - first + 1));
  }

  std::vector<std::string> Utility::split(const std::string& string)
  {
    std::vector<std::string> splits;
    for (char ch : string) {
      splits.push_back(std::string(1, ch));
    }
    return splits;
  }

  std::vector<std::string> Utility::split(const std::string& string, char delim)
  {
    std::vector<std::string> splits;
    std::string tmp;
    for (char ch : string) {
      if (ch == delim) {
	splits.push_back(tmp);
	tmp.clear();
      } else {
	tmp.push_back(ch);
      }
    }
    splits.push_back(tmp);
    return splits;
  }

  std::string Utility::to_lower(const std::string& string)
  {
    std::string result;
    for (std::string::const_iterator iter = string.begin(); iter != string.end(); iter++)
      result.push_back(std::tolower(*iter));
    return result;
  }
	
  std::string Utility::to_upper(const std::string& string)
  {
    std::string result;
    for (std::string::const_iterator iter = string.begin(); iter != string.end(); iter++)
      result.push_back(std::toupper(*iter));
    return result;
  }

  std::string Utility::config_path(std::string conf_name)
  {
    std::ifstream file;

    std::string zigurat_home_path = Utility::env_var("ZIGURATIP_HOME");
    if (zigurat_home_path.size() > 0) {
      if (zigurat_home_path.back() != '/')
	zigurat_home_path.push_back('/');
      file.open(zigurat_home_path + "etc/" + conf_name);
      if (file.good()) {
	file.close();
	return zigurat_home_path + "etc/" + conf_name;
      }
    }

    std::string user_home_path = Utility::user_home();
    if (user_home_path.size() > 0) {
      if (user_home_path.back() != '/')
	user_home_path.push_back('/');
      user_home_path += "ZiguratIP/";
      file.open(user_home_path + "etc/" + conf_name);
      if (file.good()) {
	file.close();
	return user_home_path + "etc/" + conf_name;
      }
    }

#if defined(_WIN32) || defined(_WIN64)
    std::string etc_path = "%PROGRAMDATA%/ZiguratIP/";
#else
    std::string etc_path = "/etc/ZiguratIP/";
#endif
    file.open(etc_path + conf_name);
    if (file.good()) {
      file.close();
      return etc_path + conf_name;
    }
    return "";
  }

  std::size_t Utility::generate_id()
  {
    size_t hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return std::hash<size_t>{}(hash + std::time(0));
  }

  std::string Utility::octet_as_hex(const uint8_t* octet, size_t length)
  {
    static const char hex_alphabet[] = "0123456789abcdef";
    std::string hex_string(length * 2, '0');
    for (size_t i = 0; i < length; i++) {
      hex_string[2 * i] = hex_alphabet[octet[i] / 16];
      hex_string[(2 * i) + 1] = hex_alphabet[octet[i] % 16];
    }
    return hex_string;
  }

  std::string Utility::time_to_string(time_t time, std::string format, bool is_gmt)
  {
    char buffer[256];
    struct tm* timeinfo = (is_gmt) ? gmtime(&time) : localtime(&time);
    size_t length = strftime(buffer, 255, format.c_str(), timeinfo);
    return std::string(buffer, length);
  }

  time_t Utility::string_to_time(std::string time, std::string format, bool is_gmt)
  {
    // strptime only assigns the fields the format mentions, so the rest have to
    // start from a known state rather than from whatever was on the stack.
    struct tm timeinfo;
    std::memset(&timeinfo, 0x00, sizeof(struct tm));
    timeinfo.tm_mday = 1;
    timeinfo.tm_isdst = -1;

    if (strptime(time.c_str(), format.c_str(), &timeinfo) == nullptr) return (time_t)-1;

    return (is_gmt) ? timegm(&timeinfo) : mktime(&timeinfo);
  }

  std::string Utility::pad_left(std::string word, size_t count, char ch)
  {
    if (count < word.size()) return word;
    std::string padded = word;
    padded.insert(padded.begin(), count - word.size(), ch);
    return padded;
  }

  std::string Utility::pad_right(std::string word, size_t count, char ch)
  {
    if (count < word.size()) return word;
    std::string padded = word;
    padded.insert(padded.end(), count - word.size(), ch);
    return padded;
  }

  // The platform htons/ntohl family is macro based on BSD/Darwin and lives in
  // different headers on every target, so the swaps are done here by hand.
  static bool host_is_big_endian()
  {
    const uint16_t probe = 0x0001;
    return *reinterpret_cast<const uint8_t*>(&probe) == 0x00;
  }

  uint16_t Utility::htons(uint16_t d)
  {
    if (host_is_big_endian()) return d;
    return (uint16_t)(((d & 0x00FFu) << 8) | ((d & 0xFF00u) >> 8));
  }

  uint16_t Utility::ntohs(uint16_t d)
  {
    return Utility::htons(d);
  }

  uint32_t Utility::htonl(uint32_t d)
  {
    if (host_is_big_endian()) return d;
    return ((d & 0x000000FFu) << 24) | ((d & 0x0000FF00u) <<  8) |
           ((d & 0x00FF0000u) >>  8) | ((d & 0xFF000000u) >> 24);
  }

  uint32_t Utility::ntohl(uint32_t d)
  {
    return Utility::htonl(d);
  }

  uint64_t Utility::htonll(uint64_t d)
  {
    if (host_is_big_endian()) return d;
    return (static_cast<uint64_t>(Utility::htonl((uint32_t)(d & 0xFFFFFFFFu))) << 32) |
            static_cast<uint64_t>(Utility::htonl((uint32_t)(d >> 32)));
  }

  uint64_t Utility::ntohll(uint64_t d)
  {
    return Utility::htonll(d);
  }

}
