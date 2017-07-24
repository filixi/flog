#ifndef _FLOG_FLOG_H_
#define _FLOG_FLOG_H_

#include <ctime>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "coroutine-lock.h"

namespace flog {

//! Core class of FLog, all logging stream ended here.
/*!
  BasicFLog is the core class of FLog, support multiple char types. All logging
  stream will eventually be forwarded to this class, be formatted, and be
  forwarded to the underlying basic_ostream stream.
  Every thread has it's own buffer, the buffer will be combine to the main
  buffer when thread exit.
  Each char type has its own independant underlying stream.
*/
template <class CharT, class CharTraits = std::char_traits<CharT> >
class BasicFLog {
 public:
  //! The formatting stream type.
  using ostringstream_type = std::basic_ostringstream<CharT, CharTraits>;

  //! The output stream type.
  using ostream_type = std::basic_ostream<CharT, CharTraits>;

  //! Redirects the output stream.
  /*
    Redirects the output stream.

    Parameters
      output - the new output stream

    Exception
      (none)

    Thread-safety
      thread safe
  */
  static void SetOutput(std::unique_ptr<ostream_type> output) noexcept {
    static_part_.output_ = std::move(output);
  }

  //! Pushes logging object into the stream.
  /*
    Pushes logging object into the stream.

    Parameters
      ele - the first argument to be logged. If it is convertible to
            ostringstrem_type &, it will be used as the formatting stream for
            this logging procedure.
      args - the rest argument to be logged.

    Exception
      ?

    Thread-safety
      thread safe
  */
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

  //! Forward all logs to the output stream.
  /*!
    This function will be called automatically when the program ends.
  */
  static void OutputAllLogs() {  
    auto &output = static_part_.output_ ? *static_part_.output_ : std::clog;

    for (const auto &log : static_part_.logs_)
      output << log;
  }

  //! Combined thread local logs to the main logs.
  /*!
    This function will be called automatically when the thread exits.
  */
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

  //! The static non thread local member variables.
  static struct StaticPart {
    ~StaticPart() {
      OutputAllLogs();
    }

    std::vector<std::basic_string<CharT, CharTraits> > logs_;

    std::atomic<typename ostringstream_type::fmtflags>
    fmtflags_{std::clog.flags()};

    std::unique_ptr<ostream_type> output_;

  } static_part_;

  //! The thread local member variable.
  static thread_local struct ThreadLocalPart {
    ~ThreadLocalPart() {
      MergeLocalLogsToGlobal();
    }

    std::basic_string<CharT, CharTraits> local_logs_;

  } thread_local_part_;
};


//! Definition of BasicFLog static non thread local member variable.
template <class CharT, class CharTraits>
typename BasicFLog<CharT, CharTraits>::StaticPart
BasicFLog<CharT, CharTraits>::static_part_;

//! Definition of BasicFLog thread local member variable.
template <class CharT, class CharTraits>
thread_local typename BasicFLog<CharT, CharTraits>::ThreadLocalPart
BasicFLog<CharT, CharTraits>::thread_local_part_;

//! Helper type for char stream logging
using FLog = BasicFLog<char>;

//! Helper type for wchar_t stream logging
using WFlog = BasicFLog<wchar_t>;

//! Redirects the stream of FLog
/*
  Redirects the stream of FLog. Such as to a std::ofstream. The target stream
  should be convertible to std::basic_ostream<CharT, CharTraits>, otherwise
  there will be compile time error messsage.

  Parameters
    output - the stream should be redirected to

  Exception
    ?
  
  Thread-safety
    thread safe
*/
template <template <class, class> class Stream, class CharT, class CharTraits>
void RedirectLog(
    std::unique_ptr<Stream<CharT, CharTraits> > output) {
  if constexpr (std::is_convertible<
          Stream<CharT, CharTraits> *, std::basic_ostream<CharT, CharTraits> *
        >::value) {
    BasicFLog<CharT, CharTraits>::SetOutput(
        std::unique_ptr<std::basic_ostream<CharT, CharTraits> >(
            output.release()));
  } else {
    static_assert(static_cast<CharT *>(nullptr),
                  "Redirect failed."
                  "Invalide stream type.");
  }
}

//! contains helper functions/classes (templates) for the current header
namespace detail {
//! Determines if T can be insert to std::basic_ostream<CharT, CharTraits>
template <class T, class CharT, class CharTraits,
    class = decltype(std::declval<std::basic_ostream<CharT, CharTraits> &>()
        << std::declval<T>())>
constexpr bool IsOutputible(int) {
  return true;
}

template <class...>
constexpr bool IsOutputible(...) {
  return false;
}

} // namespace detail

//! Writes logs to FLog(BasicFLog<char>)
/*!
  Writes logs to BasicFLog. If any parameter is not insertible, a pretty error
  message will be generated at compile time.

  Parameters
    args - the objects to be logged
  
  Exception
    ?
  
  Thread-safety
    thread safe
*/
template <class... Args>
FLog Log(const Args&... args) {
  constexpr bool value =
      (detail::IsOutputible<Args, char, std::char_traits<char> >(0) && ...);
  static_assert(value, "Non outputible value is not allowed");
  FLog()(args...);
  return FLog();
}


//! Writes logs to BasicFLog<CharT, CharTraits>
template <class CharT, class CharTraits, class... Args>
FLog Log(const Args&... args) {
  constexpr bool value =
      (detail::IsOutputible<Args, CharT, CharTraits>(0) && ...);
  static_assert(value, "Non outputible value is not allowed");
  BasicFLog<CharT, CharTraits>()(args...);
  return BasicFLog<CharT, CharTraits>();
}

//! Inserter of BasicFLog, forwarding all parameter to Log.
template <class CharT, class CharTraits, class X>
BasicFLog<CharT, CharTraits> &operator<<(const BasicFLog<CharT, CharTraits> &,
                                         X &&x) {
  Log<CharT, CharTraits>(std::forward<X>(x));
}

//! Inserter of BasicFLog, for supporting std::endl;
template <class CharT, class CharTraits>
BasicFLog<CharT, CharTraits> &operator<<(
    const BasicFLog<CharT, CharTraits> &,
    std::basic_ostream<CharT, CharTraits> &(&x)(
        std::basic_ostream<CharT, CharTraits> &)) {
  Log<CharT, CharTraits>(x);
}

//! Writes logs to BasicFLog<CharT, CharTraits>, adds space as spliter.
template <class CharT, class CharTraits, class... Args>
FLog LogSplit(const Args&... args) {
  (Log<CharT, CharTraits>(args, ' '), ...);
  return Log<CharT, CharTraits>("\n");
}

template <class CharT, class... Args>
FLog LogSplit(const Args&... args) {
  LogSplit<CharT, std::char_traits<CharT> >(args...);
}

template <class... Args>
FLog LogSplit(const Args&... args) {
  LogSplit<char, std::char_traits<char> >(args...);
}

template <class... Args>
FLog LogSplit(const FLog &(&f)(const FLog &), const Args&... args) {
  f(FLog()) << ' ';
  return LogSplit(args...);
}

//! Insert the current tick to the log.
const FLog &CurrentTick(const FLog &log) {
  return log << std::chrono::steady_clock::now().time_since_epoch().count();
}

//! Insert the current time in asc format to the log.
const FLog &AscTime(const FLog &log) {
  auto result = std::time(nullptr);
  return log << std::asctime(std::localtime(&result));
}

} // namespace flog

#endif // _FLOG_FLOG_H_
