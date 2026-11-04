#pragma once
#include <memory>
namespace noryn
{
// Owns an MSFA operator network and six independent envelope generators.
// No allocations occur in start(), release() or next().
class SixOperator
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    SixOperator();
    ~SixOperator();
    void prepare(double sampleRate);
    void start(int profile, float velocity, float startPhase = 0);
    void release();
    float next(float hz, float index, float ratio);
    static float preview(int profile, float phase, float index, float ratio);
};
} // namespace noryn
