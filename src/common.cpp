#include "common.hpp"

namespace rei {

[[nodiscard]] const char* getError (Result result) noexcept {
  switch (result) {
    case Result::FileDoesNotExist: return "File does not exist";
    default: return "Hmmmm";
  }
}

}
