#ifndef _BASIC_FLOG_H_
#define _BASIC_FLOG_H_

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "coroutine-lock.h"

namespace flog {
//! Core class of FLog, all logging stream will end here.
/*!
  BasicFLog is the core class of FLog, support multiple char types. All logging
  stream will eventually be forwarded to this class, be formatted, and be
  forwarded to the underlying basic_ostream stream.
  Every thread has it's own buffer, the buffer will be combined to the main
  buffer when thread exits.
  Every char type has its own independant BasicFLog class, and everything.
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
using WFLog = BasicFLog<wchar_t>;

} // namespace flog

#endif // _BASIC_FLOG_H_
