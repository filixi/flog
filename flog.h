#ifndef _FLOG_FLOG_H_
#define _FLOG_FLOG_H_

#include <atomic>
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
  BasicFLog() {
    merge_local_logs_to_global_.trigger();
    output_all_logs_.trigger();
  }

  using ostringstream_type = std::basic_ostringstream<CharT, CharTraits>;

  static void SetOutput(
      std::unique_ptr<std::basic_ostream<CharT, CharTraits> > &&output) {
    output_ = std::move(output);
  }

  template <class T, class... Args>
  typename std::enable_if<
      !std::is_convertible<T &, ostringstream_type &>::value, int>::type
  operator()(T &&ele, const Args&... args) const {
    ostringstream_type format;
    format.flags(fmtflags_.load());
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
      local_logs_ += format.str();

    return ret;
  }

  static void OutputAllLogs() {  
    auto &output = output_ ? *output_ : std::clog;

    for (const auto &log : BasicFLog<CharT, CharTraits>::logs_)
      output << log;
  }

  static void MergeLocalLogsToGlobal() {
    static std::mutex mtx;
    std::lock_guard<std::mutex> guard(mtx);
    BasicFLog<CharT, CharTraits>::logs_.push_back(
        std::move(BasicFLog<CharT, CharTraits>::local_logs_));
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

  static std::vector<std::basic_string<CharT, CharTraits> > logs_;
  static thread_local std::basic_string<CharT, CharTraits> local_logs_;

  static std::atomic<typename ostringstream_type::fmtflags> fmtflags_;

  static std::unique_ptr<std::basic_ostream<CharT, CharTraits> > output_;

  static CallOnExit output_all_logs_;
  static thread_local CallOnExit merge_local_logs_to_global_;
};

template <class CharT, class CharTraits>
std::vector<std::basic_string<CharT, CharTraits> >
BasicFLog<CharT, CharTraits>::logs_;

template <class CharT, class CharTraits>
thread_local std::basic_string<CharT, CharTraits>
BasicFLog<CharT, CharTraits>::local_logs_;

template <class CharT, class CharTraits>
std::atomic<typename BasicFLog<CharT, CharTraits>::ostringstream_type::fmtflags>
BasicFLog<CharT, CharTraits>::fmtflags_{std::clog.flags()};

template <class CharT, class CharTraits>
std::unique_ptr<std::basic_ostream<CharT, CharTraits> >
BasicFLog<CharT, CharTraits>::output_;

template <class CharT, class CharTraits>
CallOnExit BasicFLog<CharT, CharTraits>::output_all_logs_
    ([]{ BasicFLog<CharT, CharTraits>::OutputAllLogs(); });

template <class CharT, class CharTraits>
thread_local CallOnExit
BasicFLog<CharT, CharTraits>::merge_local_logs_to_global_
    ([]{ BasicFLog<CharT, CharTraits>::MergeLocalLogsToGlobal(); });

using FLog = BasicFLog<char>;
using WFlog = BasicFLog<wchar_t>;

} // namespace flog

#endif // _FLOG_FLOG_H_
