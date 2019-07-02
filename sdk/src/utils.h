#ifndef UTILS_H
#define UTILS_H

#include <atomic>
#include <stdint.h>

namespace aditof {
class Utils {
  public:
    static uint16_t *buildCalibrationCache(float gain, float offset,
                                           int16_t maxPixelValue);
    static void calibrateFrame(uint16_t *calibrationData, uint16_t *frame,
                               unsigned int width, unsigned int height);
};

class AtomicLock {
  public:
    AtomicLock(std::atomic_flag *lock) noexcept;
    ~AtomicLock() noexcept;

    /* If the return value is true it means that this AtomicLock
     * successfully set the lock to true, else it means that it was
     * already set by another thread
     */
    bool ownsLock() const;

    /* Disable copy */
    AtomicLock(const AtomicLock &) = delete;
    AtomicLock &operator=(const AtomicLock &) = delete;

    /* Disable move */
    AtomicLock(AtomicLock &&) = delete;
    AtomicLock &operator=(AtomicLock &&) = delete;

  private:
    std::atomic_flag *m_lock;
    bool m_ownsLock;
};

} // namespace aditof

#endif
