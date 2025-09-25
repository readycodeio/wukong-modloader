#pragma once
#include <utility>

template <typename F>
struct deferred_call
{
    deferred_call(F&& f) 
        : m_func(std::forward<F>(f)), m_bOwner(true) 
    {
    }

    deferred_call(const deferred_call& that) = delete;

    deferred_call(deferred_call&& that) noexcept
        : m_func(std::move(that.m_func)), m_bOwner(that.m_bOwner)
    {
        that.m_bOwner = false;
    }

    ~deferred_call()
    {
        execute();
    }

    deferred_call& operator=(deferred_call&& that) = delete;
    deferred_call& operator=(const deferred_call& that) = delete;

    bool cancel()
    {
        bool bWasOwner = m_bOwner;
        m_bOwner = false;
        return bWasOwner;
    }

    bool execute()
    {
        const auto bWasOwner = m_bOwner;

        if (m_bOwner)
        {
            m_bOwner = false;
            m_func();
        }

        return bWasOwner;
    }

private:
    F m_func;
    bool m_bOwner;
};


template <class F>
[[nodiscard]] auto make_deferred(F&& f)
    -> deferred_call<std::decay_t<F>>
{
    return deferred_call<std::decay_t<F>>(std::forward<F>(f));
}

// --- macro sugar to allow: defer([&]{ ... });
#define DEFER_CONCAT_IMPL(x,y) x##y
#define DEFER_CONCAT(x,y)      DEFER_CONCAT_IMPL(x,y)
#define DEFER(...) \
auto DEFER_CONCAT(_defer_, __LINE__) = ::make_deferred(__VA_ARGS__)