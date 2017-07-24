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

#include "call-on-exit.h"
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
  typename std::enable_if<
      !std::is_convertible<T &, ostringstream_type &>::value, int>::type
  operator()(T &&ele, const Args&... args) const {    
    static thread_local ostringstream_type format;
    format.str("");
    format.flags(static_part_.fmtflags_.load());
    return this->operator()(format, ele, args...);
  }

  template <class T, class... Args>
  typename std::enable_if<
      std::is_convertible<T &, ostringstream_type &>::value, int>::type
  operator()(T &format, const Args&... args) const {
    static thread_local CoroutineLock mtx;

    std::unique_lock<CoroutineLock> lock(mtx, std::try_to_lock);
    if (!lock.owns_lock())
      return -1;

    auto ret = AddToLog(format, args...);
    if (!ret)
      thread_local_part_.local_logs_ += format.str();

    return ret;
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
  template <class T, class... Args>
  int AddToLog(ostringstream_type &format,
               const T &ele,
               const Args&... args) const {
    if (!(format << ele))
      return -1;
    return AddToLog(format, args...);
  }

  int AddToLog(ostringstream_type &format) const {
    return 0;
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

} // namespace flog

#endif // _FLOG_FLOG_H_
