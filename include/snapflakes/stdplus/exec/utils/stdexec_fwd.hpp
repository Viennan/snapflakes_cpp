#pragma once

#include <stdexec/execution.hpp>
#include <exec/completion_signatures.hpp>

namespace snapflakes {
namespace stdplus {
namespace execution {

template <class Completions,
        class ValueFn   = experimental::execution::keep_completion<STDEXEC::set_value_t>,
        class ErrorFn   = experimental::execution::keep_completion<STDEXEC::set_error_t>,
        class StoppedFn = experimental::execution::keep_completion<STDEXEC::set_stopped_t>,
        class ExtraSigs = STDEXEC::completion_signatures<>>
consteval auto transform_completion_signatures() noexcept {
    return experimental::execution::transform_completion_signatures<Completions, ValueFn, ErrorFn, StoppedFn, ExtraSigs>();
}

}
}
}