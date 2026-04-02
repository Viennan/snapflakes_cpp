#pragma once

#include <type_traits>
#include <concepts>

namespace snapflakes {
namespace stdplus {
namespace utils {
namespace concepts {

template<typename Ele>
concept static_double_linked_listable = requires(Ele e) {
    !std::is_move_constructible_v<Ele>;
    !std::is_move_assignable_v<Ele>;
    std::is_same_v<Ele*, decltype(e.get_next())> || std::derived_from<Ele, std::remove_pointer_t<decltype(e.get_next())>>;
    std::is_same_v<Ele*, decltype(e.get_prev())> || std::derived_from<Ele, std::remove_pointer_t<decltype(e.get_prev())>>;
    { e.set_next(nullptr) };
    { e.set_prev(nullptr) };
};

}
}
}
}
