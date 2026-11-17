// Calls into mod code without letting a crash or a C++ exception unwind into ReFix.
#pragma once

#include <windows.h>
#include <type_traits>

namespace refix::mods {

namespace detail {
// No C++ objects in this frame, so __try is allowed. On MSVC a C++ exception is an SEH exception too.
inline bool SehCall(void (*thunk)(void*), void* ctx, unsigned long* code) {
    __try {
        thunk(ctx);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *code = GetExceptionCode();
        return false;
    }
}
} // namespace detail

// Runs f(); false (and the exception code) when it crashed or threw.
template <class F> bool SehGuard(F&& f, unsigned long* code) {
    using Fn = std::remove_reference_t<F>;
    unsigned long ignored = 0;
    return detail::SehCall([](void* c) { (*static_cast<Fn*>(c))(); }, &f, code ? code : &ignored);
}

} // namespace refix::mods
