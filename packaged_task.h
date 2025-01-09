//
// Created by rago on 2025/1/8.
//

#ifndef PACKEDTASK_PACKAGED_TASK_HPP
#define PACKEDTASK_PACKAGED_TASK_HPP

#include <memory>
#include <functional>
#include "lib/st.h"

template<typename _Res>
struct _Result {
public:
    _Result() noexcept: m_init(false){}

    ~_Result() {
        if (m_init) {
            value().~_Res();
        }
    }

    void set(const _Res &res) {
        ::new(addr()) _Res(res);
        m_init = true;
    }

    void set(_Res &&res) {
        ::new(addr()) _Res(std::move(res));
        m_init = true;
    }

    // Return lvalue, future will add const or rvalue-reference
    _Res &value() noexcept { return *static_cast<_Res *>(addr()); }

private:
    void *addr() noexcept { return static_cast<void *>(&m_storage); }

private:
    typedef std::alignment_of<_Res> __a_of;
    typedef std::aligned_storage<sizeof(_Res), __a_of::value> __align_storage;
    typedef typename __align_storage::type __align_type;

    __align_type m_storage;
    bool m_init;
public:
    std::exception_ptr m_exception;
};

template<typename Res>
class State_base {
protected:
    std::unique_ptr<_Result<Res>> m_result;
    st_cond_t                     m_cond;
public:
    State_base() {
        m_result = nullptr;
        m_cond = st_cond_new();
    }

    virtual ~State_base() {
        st_cond_destroy(m_cond);
    }

    State_base(const State_base &) = delete;

    State_base &operator=(const State_base &) = delete;

    _Result<Res> &wait() {
        if (m_result) {
            return *m_result;
        }
        st_cond_wait(m_cond);
        return *m_result;
    }

protected:
    void set_result(const Res &res) {
        if (m_result) {
            return;
        }
        std::unique_ptr<_Result<Res>> _local(new _Result<Res>());
        std::swap(_local, m_result);
        m_result->set(res);
        st_cond_broadcast(m_cond);
    }

    void set_exception(std::exception_ptr ptr) {
        if (m_result) {
            return;
        }

        std::unique_ptr<_Result<Res>> _local(new _Result<Res>());
        std::swap(_local, m_result);
        m_result->m_exception = ptr;
        st_cond_broadcast(m_cond);
    }
};

template<typename Res>
class Future {
    using state_type = std::shared_ptr<State_base<Res>>;

    struct _Reset {
        explicit _Reset(Future &__fut) noexcept: m_fut(__fut) {}

        ~_Reset() { m_fut.m_state.reset(); }

        Future &m_fut;
    };

public:
    Future() {
        m_state = std::make_shared<State_base<Res>>();
    }

    ~Future() = default;

    explicit Future(const state_type &state) : m_state(state) {
    }

    Future(const Future &f) = delete;

    Future &operator=(const Future &f) = delete;

    Future(Future &&_uf) noexcept {
        m_state = std::move(_uf.m_state);
    }

    Future &operator=(Future &&_fut) noexcept {
        Future(std::move(_fut)).swap(*this);
        return *this;
    }

    // Retrieving the value
    Res get() {
        _Reset reset(*this);
        _Result<Res> &_res = m_state->wait();
        if (_res.m_exception != 0) {
            std::rethrow_exception(_res.m_exception);
        }
        return std::move(_res.value());
    }

private:
    void swap(Future &_that) noexcept {
        m_state.swap(_that.m_state);
    }

private:
    state_type m_state;
};

template<typename _Signature>
class PackagedTask;

template<typename _Signature>
class _Task_state_base;

template<typename _Fn, typename _Signature>
class _Task_state;

template<typename _Res, typename... _Args>
struct _Task_state_base<_Res(_Args...)> : State_base<_Res> {
    virtual ~_Task_state_base() {}

    virtual void _M_run(_Args... __args) = 0;
};

template<typename _Fn, typename _Res, typename... _Args>
struct _Task_state<_Fn, _Res(_Args...)> final : _Task_state_base<_Res(_Args...)> {
    template<typename _Fn2>
    explicit _Task_state(_Fn2 &&__fn) : m_impl(std::forward<_Fn2>(__fn)) {}

    virtual ~_Task_state() {}

private:
    template<typename _Tp>
    static std::reference_wrapper<_Tp>
    _S_maybe_wrap_ref(_Tp &__t) { return std::ref(__t); }

    template<typename _Tp>
    static typename std::enable_if<!std::is_lvalue_reference<_Tp>::value, _Tp>::type &&
    _S_maybe_wrap_ref(_Tp &&__t) { return std::forward<_Tp>(__t); }

    virtual void _M_run(_Args... __args) {
        // bound arguments decay so wrap lvalue references
        auto boundfn = std::bind(std::ref(m_impl.m_fn), _S_maybe_wrap_ref(std::forward<_Args>(__args))...);
        try {
            this->set_result(boundfn());
        } catch (...) {
            this->set_exception(std::current_exception());
        }
    }

private:
    struct _Impl {
        template<typename _Fn2>
        _Impl(_Fn2 &&__fn) :  m_fn(std::forward<_Fn2>(__fn)) {}

        _Fn m_fn;
    } m_impl;
};

template<typename _Signature, typename _Fn>
static std::shared_ptr<_Task_state_base<_Signature>> __create_task_state(_Fn &&__fn) {
    typedef typename std::decay<_Fn>::type _Fn2;
    typedef _Task_state<_Fn2, _Signature> _State;
    return std::make_shared<_State>(std::forward<_Fn>(__fn));
}

// 模版约束类型检查
template<typename _Task, typename _Fn, bool = std::is_same<_Task, typename std::decay<_Fn>::type>::value>
struct __constrain_pkgdtask {
    typedef void __type;
};

template<typename _Task, typename _Fn>
struct __constrain_pkgdtask<_Task, _Fn, true> {
};

// 类似std::packaged_task封装任务, 暂时不支持void返回类型
template<typename _Res, typename... _ArgTypes>
class PackagedTask<_Res(_ArgTypes...)> {
    typedef _Task_state_base<_Res(_ArgTypes...)> _State_type;
    std::shared_ptr<_State_type> m_state;
public:
    PackagedTask() : m_state(nullptr) {}

    template<typename _Fn, typename = typename __constrain_pkgdtask<PackagedTask, _Fn>::__type>
    explicit PackagedTask(_Fn &&__fn):m_state(__create_task_state<_Res(_ArgTypes...)>(
            std::forward<_Fn>(__fn))) {}

    ~PackagedTask() {}

    PackagedTask(const PackagedTask &) = delete;

    PackagedTask &operator=(const PackagedTask &) = delete;

    PackagedTask(PackagedTask &&__other) noexcept { this->swap(__other); }

    PackagedTask &operator=(PackagedTask &&__other) noexcept {
        PackagedTask(std::move(__other)).swap(*this);
        return *this;
    }

    // 执行函数
    void operator()(_ArgTypes... __args) {
        m_state->_M_run(std::forward<_ArgTypes>(__args)...);
    }

    Future<_Res> get_future() { return Future<_Res>(m_state); }

private:
    void swap(PackagedTask &__other) noexcept { m_state.swap(__other.m_state); }
};

#endif //PACKEDTASK_PACKAGED_TASK_HPP
