#ifndef INCLUDED_RSANS_IO
#define INCLUDED_RSANS_IO

#include <string>

std::string readFile(const std::string& path);
void writeFile(const std::string& path, const std::string& data);

#endif
