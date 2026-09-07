#pragma once
#include <algorithm>
namespace wakeai {
// UI-thread coordinator; action objects belong to the recognition worker.
class WorkoutSession {
public:
    void begin(int target) {
        target_ = std::max(1, target); count_ = 0;
        active_ = true; reached_ = false; finished_ = false;
    }
    bool updateCount(int value) {
        if (!active_ || reached_) return false;
        count_ = std::max(count_, std::max(0, value));
        if (count_ >= target_) { reached_ = true; return true; }
        return false;
    }
    bool finish() {
        if (!active_ || !reached_ || finished_) return false;
        finished_ = true; active_ = false; return true;
    }
    void cancel() { active_ = false; }
    int count() const { return count_; }
    int target() const { return target_; }
    bool reached() const { return reached_; }
    bool finished() const { return finished_; }
private:
    int target_ = 1, count_ = 0;
    bool active_ = false, reached_ = false, finished_ = false;
};
}
