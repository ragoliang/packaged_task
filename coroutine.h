//
// Created by rago on 2024/5/20.
//

#ifndef LIVE_STREAM_PROXY2_0_COROUTINE_HPP
#define LIVE_STREAM_PROXY2_0_COROUTINE_HPP
#ifdef __cplusplus
extern "C" {
#endif

#include "lib/st.h"

#ifdef __cplusplus
}
#endif

#include <functional>
#include <memory>

class Coroutine {
private:
    struct _Impl_base;
    typedef std::shared_ptr<_Impl_base> __shared_base_type;

    class Cid {
        st_thread_t _M_thread;
    public:
        Cid() noexcept: _M_thread(nullptr) {}

        explicit Cid(st_thread_t __id) : _M_thread(__id) {}

    private:
        friend class Coroutine;

        friend bool operator==(Coroutine::Cid __x, Coroutine::Cid __y) noexcept {
            return __x._M_thread == __y._M_thread;
        }

        friend bool operator<(Coroutine::Cid __x, Coroutine::Cid __y) noexcept { return __x._M_thread < __y._M_thread; }

        template<class _CharT, class _Traits>
        friend std::basic_ostream<_CharT, _Traits> &
        operator<<(std::basic_ostream<_CharT, _Traits> &__out, Coroutine::Cid __id) {
            if (__id == Coroutine::Cid())
                return __out << "thread::id of a non-executing thread";
            else
                return __out << __id._M_thread;
        }
    };

    struct _Impl_base {
        __shared_base_type _M_this_ptr;

        virtual ~_Impl_base() {}

        virtual void _M_run() = 0;
    };


    template<typename Callable>
    struct _Impl : public _Impl_base {
        Callable _M_func;
        _Impl(Callable &&__f) : _M_func(std::forward<Callable>(__f)) {
        }

        ~_Impl() override {
        }

        void _M_run() override {
            _M_func();
        }
    };

    template<typename Callable>
    static std::shared_ptr<_Impl<Callable>> _M_make_routine(Callable &&__f) {
        return std::make_shared<_Impl< Callable>>(std::forward<Callable>(__f));
    }

public:
    typedef unsigned long thread_id_t;
    Coroutine() = default;
    template<typename Callable, typename...  Args>
    explicit Coroutine(Callable &&__f, Args &&... __args) {
        _M_start_thread(_M_make_routine(std::bind(
                std::forward<Callable>(__f),
                std::forward<Args>(__args)...)));
    }

    ~Coroutine() {
        // 协构的时候，不应该是可以被join状态
        if (joinable()) {
            std::terminate();
        }
    }

    Coroutine &operator=(const Coroutine &) = delete;

    Coroutine(const Coroutine &r) = delete;

    Coroutine(Coroutine &&__t) noexcept {
        swap(__t);
    }

    Coroutine &operator=(Coroutine &&__t) noexcept {
        if (joinable()) {
            std::terminate();
        }
        swap(__t);
        return *this;
    }

    thread_id_t get_cid() {
        return (thread_id_t) _M_id._M_thread;
    }

    void interrupt() {
        if (!_M_id._M_thread) {
            return;
        }
        // 如果协程正在阻塞中，中断
        st_thread_interrupt(_M_id._M_thread);
    }

    void join() {
        if (!joinable()) {
            return;
        }
        st_thread_join(_M_id._M_thread, nullptr);
        _M_id = Cid();
    }

    void detach() {
        if (!_M_id._M_thread) {
            return;
        }
        if (_M_id._M_thread == st_thread_self()) {
            return;
        }
        // _M_id._M_thread = nullptr
        _M_id = Cid();
    }
private:
    static void *execute_native_thread_routine(void *__p) {
        Coroutine::_Impl_base *__t = static_cast<Coroutine::_Impl_base *>(__p);
        Coroutine::__shared_base_type __local;
        __local.swap(__t->_M_this_ptr);
        __t->_M_run();
        return nullptr;
    }

    void _M_start_thread(__shared_base_type __b) {
        __b->_M_this_ptr = __b;
        _M_id._M_thread = st_thread_create(execute_native_thread_routine, __b.get(), 1, 0);
        if (!_M_id._M_thread) {
            __b->_M_this_ptr.reset();
            throw std::runtime_error("can not create coroutine");
        }
    }

    void swap(Coroutine &__t) {
        std::swap(_M_id, __t._M_id);
    }

    bool joinable() {
        if (!_M_id._M_thread) {
            return false;
        }
        return _M_id._M_thread != st_thread_self();
    }

private:
    Cid _M_id;
};


#endif //LIVE_STREAM_PROXY2_0_COROUTINE_HPP
