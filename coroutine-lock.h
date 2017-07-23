#ifndef _COROUTINE_LOCK_H_
#define _COROUTINE_LOCK_H_

class CoroutineLock {
 public:
  void lock() {
    if (flag_)
      throw std::runtime_error("Coroutine lock failed.");
    flag_ = true;
  }

  bool try_lock() noexcept {
    if (flag_ == true)
      return false;
    return flag_ = true;
  }

  void unlock() {
    if (flag_ == false)
      throw std::runtime_error("Unlocking unlocked coroutine lock.");
    flag_ = false;
  }

  ~CoroutineLock() noexcept {
    if (flag_ == true)
      exit(-1); // destroys a lock which owns the lock.
  }

 private:
  volatile bool flag_ = false;
};

#endif // _COROUTINE_LOCK_H_
