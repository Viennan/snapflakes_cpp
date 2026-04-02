#pragma once

#include <mutex>
#include <snapflakes/stdplus/exec/std.hpp>

namespace snapflakes {
namespace stdplus {
namespace streamifex {

template<typename Snd, typename Mutex = std::mutex, typename Env = stdexec::env<>>
struct be_eager {

};

}
}
}