/*
logging.cpp
 JP Robichaud (jp@rev.com)
 2018

*/

#include "logging.h"

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

namespace logger {

std::string CONSOLE_LOGGER_NAME = "console";

std::vector<spdlog::sink_ptr> sinks;

void InitLoggers(std::string logfilename) {
  sinks.push_back(std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>());
  if (logfilename.size() > 0) {
    auto filesink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfilename);
    sinks.push_back(filesink);
  }

  spdlog::set_level(spdlog::level::info);
  auto console = std::make_shared<spdlog::logger>(CONSOLE_LOGGER_NAME, begin(sinks), end(sinks));
  spdlog::register_logger(console);

  //   auto console = spd::stdout_color_mt(CONSOLE_LOGGER_NAME);
  console->info("loggers initialized");
  console->flush_on(spdlog::level::info);
  // in general, we can have the utc offset, but for stdout, let's be lean a bit
  spdlog::set_pattern("[%^+++%$] [%H:%M:%S %z] [thread %t] [%n] %v");
  console->set_pattern("[%^+++%$] [%H:%M:%S] [%n] %v");

  // todo : define extra loggers for individual components and read their levels
  // from a trc.cfg
}

std::shared_ptr<spdlog::logger> GetOrCreateLogger(std::string name) {
  auto log = spdlog::get(name);

  if (log == nullptr) {
    // since we'll go to stdout, we'll avoid the utc offset
    // log = spdlog::stdout_color_mt(name);
    log = std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
    spdlog::register_logger(log);
    log->flush_on(spdlog::level::info);
    log->set_pattern("[%^+++%$] [%H:%M:%S] [%n] %v");
  }

  return log;
}

std::shared_ptr<spdlog::logger> GetLogger(std::string name) {
  auto log = spdlog::get(name);

  if (log == nullptr) {
    log = spdlog::get(CONSOLE_LOGGER_NAME);
    log->error(
        "The requested logger name [{}] wasn't found in the registery, using "
        "[{}] instead",
        name, CONSOLE_LOGGER_NAME);
  }

  return log;
}

void CloseLoggers() {
  // closing everything
  spdlog::drop_all();
}
}  // namespace logger
