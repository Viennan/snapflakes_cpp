#pragma once

#include <type_traits>

namespace snapflakes {
namespace stdplus {
namespace utils {

template<typename T>
requires std::is_integral_v<T>
class non_atomic{
public:


private:
    T value_ = 0;
};

}
}
}