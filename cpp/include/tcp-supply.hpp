#pragma once

#include <functional>
#include <string>

namespace TCP {

enum MessagePriority { Error = 0, Warning = 1, Info = 2, Debug = 3 };
using logging_foo = std::function<void(const std::string&, const std::string&,
                                       const std::string&, int priority)>;

}  // namespace TCP