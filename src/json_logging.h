/*
json_logging.h
 Nishchal Bhandari (nishchal@rev.com)
 2019

*/

#ifndef __JSONLOGGING_H__
#define __JSONLOGGING_H__

#include <json/json.h>

namespace jsonLogger {

class JsonLogger {
 private:
  JsonLogger() {}

 public:
  Json::Value root;

  static JsonLogger& getLogger() {
    static JsonLogger instance;
    return instance;
  }

  JsonLogger(JsonLogger const&) = delete;
  void operator=(JsonLogger const&) = delete;
};

}  // namespace jsonLogger

#endif  // __JSONLOGGING_H__
