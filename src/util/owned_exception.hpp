#ifndef GODOT_AUTOPILOT_OWNED_EXCEPTION_HPP
#define GODOT_AUTOPILOT_OWNED_EXCEPTION_HPP

#include <stdexcept>
#include <string>
#include <utility>

namespace godot_autopilot {

namespace detail {

template <typename Base>
class OwnedMessageException : public Base {
public:
  explicit OwnedMessageException(std::string message)
      : Base(message.c_str()), message_(std::move(message)) {}

  const char *what() const noexcept override { return message_.c_str(); }

private:
  std::string message_;
};

} // namespace detail

using RuntimeError = detail::OwnedMessageException<std::runtime_error>;
using InvalidArgument = detail::OwnedMessageException<std::invalid_argument>;

} // namespace godot_autopilot

#endif
