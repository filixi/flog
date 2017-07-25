#ifndef _FLOG_FLOG_H_
#define _FLOG_FLOG_H_

#include <ctime>

#include <chrono>
#include <ostream>
#include <type_traits>
#include <utility>

#include "basic-flog.h"

namespace flog {
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
  static_assert(value, "All values must be insertible to std stream.");
  FLog()(args...);
  return FLog();
}

//! Writes logs to BasicFLog<CharT, CharTraits>
template <class CharT, class CharTraits, class... Args>
FLog Log(const Args&... args) {
  constexpr bool value =
      (detail::IsOutputible<Args, CharT, CharTraits>(0) && ...);
  static_assert(value, "All values must be insertible to std stream.");
  BasicFLog<CharT, CharTraits>()(args...);
  return BasicFLog<CharT, CharTraits>();
}

//! Inserter of BasicFLog, forwarding all parameter to Log.
template <class CharT, class CharTraits, class X>
BasicFLog<CharT, CharTraits> &operator<<(const BasicFLog<CharT, CharTraits> &,
                                         X &&x) {
  Log<CharT, CharTraits>(std::forward<X>(x));
}

//! Inserter of BasicFLog, for supporting std::endl.
template <class CharT, class CharTraits>
BasicFLog<CharT, CharTraits> &operator<<(
    const BasicFLog<CharT, CharTraits> &,
    std::basic_ostream<CharT, CharTraits> &(&x)(
        std::basic_ostream<CharT, CharTraits> &)) {
  Log<CharT, CharTraits>(x);
}

//! Inserter of BasicFlog, for flog extension.
template <class CharT, class CharTraits>
BasicFLog<CharT, CharTraits> &operator<<(
    const BasicFLog<CharT, CharTraits> &,
    const BasicFLog<CharT, CharTraits> &(&f)(
        const BasicFLog<CharT, CharTraits> &)) {
  f(BasicFLog<CharT, CharTraits>());
}

//! Writes logs to BasicFLog<CharT, CharTraits>, adds space as spliter.
template <class CharT, class CharTraits, class... Args>
FLog LogSplit(const Args&... args) {
  (Log<CharT, CharTraits>(args, ' '), ...);
  return Log<CharT, CharTraits>("\n");
}

//! LogSplit overloading for CharT, specified by the first template argument.
template <class CharT, class... Args>
FLog LogSplit(const Args&... args) {
  LogSplit<CharT, std::char_traits<CharT> >(args...);
}

//! LogSplit overloading for char type
template <class... Args>
FLog LogSplit(const Args&... args) {
  LogSplit<char, std::char_traits<char> >(args...);
}

//! LogSplit overloading for flog extension, char type.
template <class... Args>
FLog LogSplit(const FLog &(&f)(const FLog &), const Args&... args) {
  f(FLog()) << ' ';
  return LogSplit(args...);
}

//! Insert the current tick to the log.
template <class CharT, class Traits>
const BasicFLog<CharT, Traits> &CurrentTick(
    const BasicFLog<CharT, Traits> &log) {
  return log << std::chrono::steady_clock::now().time_since_epoch().count();
}

//! Insert the current time in asc format to the log.
/*!
  Supports only char type.
*/
const FLog &AscTime(const FLog &log) {
  std::time_t result = std::time(nullptr);
  char buff[128];
  strftime(buff, sizeof(buff), "%d-%m-%Y %H:%M:%S", std::localtime(&result));
  return log << buff;
}

} // namespace flog

#endif // _FLOG_FLOG_H_
