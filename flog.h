#ifndef _FLOG_FLOG_H_
#define _FLOG_FLOG_H_

#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <iostream>
#include <vector>

#include "coroutine-lock.h"

namespace flog {

template <class CharT, class CharTraits = std::char_traits<CharT> >
class BasicFLog {
 public:
  using ostringstream_type = std::basic_ostringstream<CharT, CharTraits>;

  static void SetOutput(
      std::unique_ptr<std::basic_ostream<CharT, CharTraits> > &&output) {
    static_part_.output_ = std::move(output);
  }

  template <class T, class... Args>
  void operator()(T &&ele, const Args&... args) const {    
    static thread_local ostringstream_type format;
    static thread_local CoroutineLock mtx;

    std::unique_lock<CoroutineLock> lock(mtx, std::try_to_lock);
    if (!lock.owns_lock())
      return ;

    if constexpr (!std::is_convertible<T &, ostringstream_type &>::value) {
      format.str("");
      format.flags(static_part_.fmtflags_.load());

      if (AddToLog(format, ele, args...))
        return ;
      thread_local_part_.local_logs_ += format.str();
    } else {
      if (AddToLog(ele, args...))
        return ;
      thread_local_part_.local_logs_ +=
          static_cast<ostringstream_type &>(ele).str();
    }
  }

  static void OutputAllLogs() {  
    auto &output = static_part_.output_ ? *static_part_.output_ : std::clog;

    for (const auto &log : static_part_.logs_)
      output << log;
  }

  static void MergeLocalLogsToGlobal() {
    static std::mutex mtx;
    std::lock_guard<std::mutex> guard(mtx);
    static_part_.logs_.emplace_back(
        std::move(thread_local_part_.local_logs_));
  }

 private:
  template <class... Args>
  int AddToLog(ostringstream_type &format,
               const Args&... args) const {
    (format << ... << args);
    return format.good() ? 0 : -1;
  }

  static struct StaticPart {
    ~StaticPart() {
      OutputAllLogs();
    }

    std::vector<std::basic_string<CharT, CharTraits> > logs_;

    std::atomic<typename ostringstream_type::fmtflags>
    fmtflags_{std::clog.flags()};

    std::unique_ptr<std::basic_ostream<CharT, CharTraits> > output_;

  } static_part_;

  static thread_local struct ThreadLocalPart {
    ~ThreadLocalPart() {
      MergeLocalLogsToGlobal();
    }

    std::basic_string<CharT, CharTraits> local_logs_;

  } thread_local_part_;
};

template <class CharT, class CharTraits>
typename BasicFLog<CharT, CharTraits>::StaticPart
BasicFLog<CharT, CharTraits>::static_part_;

template <class CharT, class CharTraits>
thread_local typename BasicFLog<CharT, CharTraits>::ThreadLocalPart
BasicFLog<CharT, CharTraits>::thread_local_part_;

using FLog = BasicFLog<char>;
using WFlog = BasicFLog<wchar_t>;

FLog log;
WFlog wlog; 

template <class CharT, class CharTraits>
void RedirectLog(
    std::unique_ptr<std::basic_ostream<CharT, CharTraits> > output) {
  BasicFLog<CharT, CharTraits>::SetOutput(std::move(output));
}

template <class... Args>
FLog Log(const Args&... args) {
  FLog()(args...);
  return FLog();
}

template <class... Args>
FLog LogSplit(const Args&... args) {
  (Log(args, ' '), ...);
  return Log('\n');
}

template <class... Args>
FLog LogSplit(uint64_t (&f)(int), const Args&... args) {
  return LogSplit(f(0), args...);
}

template <class X>
uint64_t CurrentTick(X) {
  return std::chrono::steady_clock::now().time_since_epoch().count();
}

template <class CharT, class CharTraits, class X>
BasicFLog<CharT, CharTraits> &operator<<(const BasicFLog<CharT, CharTraits> &,
                                         X &&x) {
  Log(x);
}

template <class CharT, class CharTraits>
BasicFLog<CharT, CharTraits> &operator<<(
    const BasicFLog<CharT, CharTraits> &,
    std::basic_ostream<CharT, CharTraits> &(&x)(
        std::basic_ostream<CharT, CharTraits> &)) {
  Log(x);
}

} // namespace flog

#endif // _FLOG_FLOG_H_
