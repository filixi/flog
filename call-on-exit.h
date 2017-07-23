#ifndef _CALL_ON_EXIT_
#define _CALL_ON_EXIT_

#include <functional>

class CallOnExit {
 public:
  CallOnExit() = delete;
  CallOnExit(const CallOnExit &) = delete;

  template <class Fn>
  explicit CallOnExit(Fn &&fn) noexcept : call_on_destroy_(std::forward<Fn>(fn)) {}

  CallOnExit &operator=(const CallOnExit &) = delete;

  void trigger() volatile {}

  ~CallOnExit() noexcept {
    try {
      call_on_destroy_();
    } catch(...) {}
  }

 private:
  std::function<void(void)> call_on_destroy_;
};

#endif // _CALL_ON_EXIT_
