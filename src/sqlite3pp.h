#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <sqlite3.h>
#include <stdint.h>

namespace detail {

inline void mk_parent_dir(std::string_view path) {
    std::filesystem::path fp(path.data());
    std::filesystem::create_directories(fp.parent_path());
}

template <typename T>
struct is_optional : public std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : public std::true_type {};

template <typename T>
constexpr bool is_optional_v = is_optional<T>::value;

template <typename T>
struct is_tuple : public std::false_type {};

template <typename... Args>
struct is_tuple<std::tuple<Args...>> : public std::true_type {};

template <typename T>
constexpr bool is_tuple_v = is_tuple<T>::value;

}  // namespace detail

class Sqlite3pp final {
public:
    Sqlite3pp() : closed_(false), auto_commit_(true), conn_(nullptr) {}

    ~Sqlite3pp() { close(); }

    void open(std::string_view sourcename, int timeout = -1) {
        std::lock_guard<std::mutex> _lck(mtx_);
        if (conn_) {
            throw std::runtime_error("sqlite3 already opened !!!");
        }

        int mode = SQLITE_OPEN_READWRITE;
        if (sourcename == ":memory:") {
            mode |= SQLITE_OPEN_MEMORY;
        } else {
            mode |= SQLITE_OPEN_CREATE;
            detail::mk_parent_dir(sourcename.data());
        }
        int res = sqlite3_open_v2(sourcename.data(), &conn_, mode, nullptr);
        if (res != SQLITE_OK) {
            auto errmsg = sqlite3_errmsg(conn_);
            sqlite3_close_v2(conn_);
            conn_ = nullptr;
            throw std::runtime_error(std::string("sqlite3_open_v2:") + errmsg);
        }

        if (timeout > 0) {
            sqlite3_busy_timeout(conn_, timeout);
        }

        // pragma
        unsafe_execute("PRAGMA journal_mode=WAL;");
        unsafe_execute("PRAGMA synchronous=NORMAL;");
        unsafe_execute("PRAGMA case_sensitive_like=ON;");
        unsafe_execute("PRAGMA locking_mode=NORMAL;");

        closed_ = false;
    }

    void close() {
        std::lock_guard<std::mutex> _lck(mtx_);
        if (!closed_) {
            closed_ = true;
            if (conn_) {
                sqlite3_close_v2(conn_);
                conn_ = nullptr;
            }
        }
    }

    decltype(auto) make_lock() {
        return std::make_unique<std::lock_guard<std::mutex>>(mtx_);
    }

    template <typename R = void, typename... Args>
    decltype(auto) execute(std::string_view stmt, Args&&... args) {
        std::lock_guard<std::mutex> _lck(mtx_);
        return unsafe_execute<R>(stmt, std::forward<Args>(args)...);
    }

    template <typename R = void, typename... Args>
    std::enable_if_t<!std::is_void_v<R>, std::vector<R>>
    unsafe_execute(std::string_view stmt, Args&&... args) {
        return execute_impl<R>(stmt, std::forward<Args>(args)...);
    }

    template <typename R = void, typename... Args>
    std::enable_if_t<std::is_void_v<R>>
    unsafe_execute(std::string_view stmt, Args&&... args) {
        return execute_impl<void>(stmt, std::forward<Args>(args)...);
    }

private:
    template <typename T>
    static void sqlite3pp_bind(sqlite3_stmt* vm, int param_no, T&& t) {
        using T0 = std::decay_t<T>;
        int rc   = 0;

        if constexpr (std::is_integral_v<T0>) {
            if constexpr (sizeof(T0) <= 4) {
                rc = sqlite3_bind_int(vm, param_no, static_cast<int>(std::forward<T>(t)));
            } else {
                rc = sqlite3_bind_int64(vm, param_no,
                                        static_cast<int64_t>(std::forward<T>(t)));
            }
        } else if constexpr (std::is_same_v<T0, float> || std::is_same_v<T0, double>) {
            rc = sqlite3_bind_double(vm, param_no, std::forward<T>(t));
        } else if constexpr (std::is_same_v<T0, const char*>) {
            rc = sqlite3_bind_text(vm, param_no, t, -1, SQLITE_TRANSIENT);
        } else if constexpr (std::is_same_v<T0, std::string>
                             || std::is_same_v<T0, std::string_view>) {
            // rc                 = sqlite3_bind_null(vm, param_no);
            rc = sqlite3_bind_text(vm, param_no, t.data(), t.size(), SQLITE_TRANSIENT);
        } else if constexpr (std::is_same_v<T0, std::vector<char>>) {
            rc = sqlite3_bind_text(vm, param_no, t.data(), t.size(), SQLITE_TRANSIENT);
        } else if constexpr (detail::is_optional_v<T0>) {
            if (!t.has_value()) {
                rc = sqlite3_bind_null(vm, param_no);
            } else {
                using T1 = typename T0::value_type;
                T1 t1    = t.value();
                sqlite3pp_bind<T1>(vm, param_no, std::move(t1));
            }
        } else {
            throw std::runtime_error("Unknown sqlite3 type !!!");
        }

        if (rc != SQLITE_OK) {
            throw std::runtime_error(std::string("sqlite3_bind err:") + std::to_string(rc));
        }
    }

    template <typename T>
    static void sqlite3pp_column(sqlite3_stmt* vm, int column, T& t) {
        if constexpr (std::is_integral_v<T>) {
            if constexpr (sizeof(t) <= 4) {
                t = sqlite3_column_int(vm, column);
            } else {
                t = sqlite3_column_int64(vm, column);
            }
        } else if constexpr (std::is_same_v<double, T> || std::is_same_v<T, float>) {
            t = sqlite3_column_double(vm, column);
        } else if constexpr (std::is_same_v<T, std::string>) {
            auto s  = (const char*)sqlite3_column_text(vm, column);
            int len = sqlite3_column_bytes(vm, column);
            t       = std::string(s, len);
        } else if constexpr (std::is_same_v<T, std::vector<char>>) {
            auto blob = (const char*)sqlite3_column_blob(vm, column);
            int len   = sqlite3_column_bytes(vm, column);
            t         = std::vector<char>(blob, blob + len);
        } else if constexpr (detail::is_optional_v<T>) {
            using T0 = typename T::value_type;
            T0 t0;
            sqlite3pp_column(vm, column, t0);
            t = t0;
        } else {
            throw std::runtime_error("Unknown sqlite3 type !!!");
        }
    }

    /*
  ** Execute an SQL statement.
  ** Return a Cursor object if the statement is a query, otherwise
  ** return the number of tuples affected by the statement.
  */
    template <typename R, typename... Args>
    std::conditional_t<std::is_void_v<R>, void, std::vector<R>>
    execute_impl(std::string_view stmt, Args&&... args) {
        int res;
        sqlite3_stmt* vm;
        const char* tail;

        check_conn();

        res = sqlite3_prepare_v3(conn_, stmt.data(), -1, 0, &vm, &tail);
        if (res != SQLITE_OK) {
            throw_except("sqlite3_prepare_v3");
        }

        if constexpr (sizeof...(args) > 0) {
            int index = 1;
            ((sqlite3pp_bind(vm, index++, std::forward<Args>(args))), ...);
        }

        // 执行SQL语句并遍历结果集
        if constexpr (detail::is_tuple_v<R>) {
            std::vector<R> ret;
            while ((res = sqlite3_step(vm)) == SQLITE_ROW) {
                R e;
                auto f = [&](auto&... e0) {
                    int index = 0;
                    ((sqlite3pp_column<std::decay_t<decltype(e0)>>(vm, index++, e0)), ...);
                };
                std::apply(f, e);
                ret.emplace_back((R&&)e);
            }

            if (res != SQLITE_DONE) {
                throw_except("execute");
            }

            sqlite3_finalize(vm);
            return ret;
        } else if constexpr (std::is_same_v<R, void>) {
            while ((res = sqlite3_step(vm)) == SQLITE_ROW) {
            }

            if (res != SQLITE_DONE) {
                throw_except("execute");
            }

            sqlite3_finalize(vm);
        } else {
            throw std::runtime_error("R type error !!!");
        }
    }

    inline void check_conn() {
        if (!conn_) {
            throw std::runtime_error("Expect connection !!!");
        }
    }

    inline void throw_except(std::string_view what = "") {
        if (!conn_) {
            return;
        }

        std::string msg;
        if (what.empty()) {
            msg = sqlite3_errmsg(conn_);
        } else {
            msg = std::string(what) + ": " + sqlite3_errmsg(conn_);
        }
        throw std::runtime_error(msg);
    }

private:
    std::mutex mtx_;
    bool closed_;
    bool auto_commit_;
    sqlite3* conn_;
};
