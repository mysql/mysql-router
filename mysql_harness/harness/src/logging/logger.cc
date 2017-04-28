/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/logging/logger.h"
#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/registry.h"

namespace mysql_harness {

namespace logging {


////////////////////////////////////////////////////////////////
// class Logger

Logger::Logger(Registry& registry, LogLevel level)
    : level_(level), registry_(&registry) {}

void Logger::attach_handler(std::string name) {
  // verification that the handler actually exists will be performed in
  // Registry::update_logger() - it makes no sense to do it earlier, since it
  // can still change between now and the time when we update_logger() is called
  handlers_.emplace(name);
}

void Logger::detach_handler(std::string name, bool handler_must_exist /*= true */) {
  if (handlers_.erase(name) == 0 && handler_must_exist)
    throw std::logic_error(std::string("Detaching unknown handler '") + name + "'");
}

void Logger::handle(const Record& record) {
  if (record.level <= level_) {
    for (const std::string& handler_id : handlers_) {
      std::shared_ptr<Handler> handler;
      try {
        handler = registry_->get_handler(handler_id.c_str());
      } catch (std::logic_error) {
        // It may happen that another thread has removed this handler since
        // we got a copy of our Logger object, and we now have a dangling
        // reference. In such case, simply skip it.
        continue;
      }
      if (record.level <= handler->get_level())
        handler->handle(record);
    }
  }
}

} // namespace logging

} // namespace mysql_harness
