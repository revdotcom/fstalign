/*
logging.h
 JP Robichaud (jp@rev.com)
 2018

*/

#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <spdlog/spdlog.h>

#define HERE_FMT "{}:{:d}: "
#define HERE2 __FILE__, __LINE__
#define HEREF2 __FUNCTION__, __LINE__

namespace logger {

namespace spd = spdlog;

void InitLoggers(std::string logfilename);
std::shared_ptr<spd::logger> GetOrCreateLogger(std::string name);
std::shared_ptr<spd::logger> GetLogger(std::string name);
void CloseLoggers();
}  // namespace logger

#endif  // __LOGGING_H__
