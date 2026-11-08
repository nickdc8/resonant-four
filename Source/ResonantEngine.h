#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Circuit-informed behaviour; see research/RESEARCH.md for provenance and omissions.
// No JUCE dependency: this exact engine is exercised by the offline stress tests.
namespace resonant {
struct Controls {
    int mode = 0; // Flange, Chorus, Filter Matrix, Double Track
    float rate = .55f, width = .35f, tune = .9f, feedback = .5f;
    float sweep = 0, wetOnly = 0, bypass = 0, outputDb = -6;
};
class Engine {
public:
    void prepare(double sampleRate) {
        sr = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 44100.0;
        for (auto& c : channels) c.delay.assign(static_cast<size_t>(sr * .14) + 8, 0.f);
        paramAlpha = static_cast<float>(1 - std::exp(-1 / (.03 * sr)));
        delayAlpha = static_cast<float>(1 - std::exp(-1 / (.01 * sr)));
        dcPole = static_cast<float>(std::exp(-2 * pi * 12 / sr));
        inPole = static_cast<float>(std::exp(-2 * pi * 25 / sr));
        preLp = static_cast<float>(1 - std::exp(-2 * pi * std::min(9000.0, .45 * sr) / sr));
        fadeStep = static_cast<float>(1 / (.01 * sr));
        reset();
    }
    void reset() {
        clearLoops(); phase = 0; mode = std::clamp(target.mode, 0, 3);
        current = target; modeGain = 1; delaySeconds = static_cast<float>(delayFor(current, mode, 0));
    }
    void setControls(Controls c) {
        c.mode = std::clamp(c.mode, 0, 3);
        c.rate = clean(c.rate, .1f, 10.f); c.width = clean(c.width, 0, 1);
        c.tune = clean(c.tune, 0, 1); c.feedback = clean(c.feedback, 0, 1);
        c.sweep = clean(c.sweep, 0, 1); c.wetOnly = clean(c.wetOnly, 0, 1);
        c.bypass = clean(c.bypass, 0, 1); c.outputDb = clean(c.outputDb, -24, 6);
        target = c;
    }
    std::array<float, 2> process(float left, float right) {
        smooth(current.rate, target.rate); smooth(current.width, target.width);
        smooth(current.tune, target.tune); smooth(current.feedback, target.feedback);
        smooth(current.sweep, target.sweep); smooth(current.wetOnly, target.wetOnly);
        smooth(current.outputDb, target.outputDb);
        current.bypass += std::clamp(target.bypass - current.bypass, -fadeStep, fadeStep);
        if (mode != target.mode) {
            modeGain = std::max(0.f, modeGain - fadeStep);
            if (modeGain <= 0) {
                mode = target.mode; clearLoops();
                delaySeconds = static_cast<float>(delayFor(current, mode, phase));
            }
        } else modeGain = std::min(1.f, modeGain + fadeStep);
        phase += current.rate / sr; phase -= std::floor(phase);
        delaySeconds += delayAlpha * (static_cast<float>(delayFor(current, mode, phase)) - delaySeconds);
        const float delaySamples = std::clamp(delaySeconds * static_cast<float>(sr), 2.f,
                                              static_cast<float>(channels[0].delay.size() - 3));
        // Bandwidth follows the BBD clock: a 512-stage section (comb modes) or two chained
        // (echo modes) needs a slower clock for longer delay, so long echoes darken and short
        // ones stay bright. Stage count and the 9 kHz ceiling are assumptions, not measurements.
        const bool echoFamily = mode == 1 || mode == 3;
        const float nyquist = (echoFamily ? 1024.f : 512.f) / (4.f * std::max(delaySeconds, 1e-4f));
        const float cutoff = std::min({.8f * nyquist, 9000.f, .45f * static_cast<float>(sr)}) * (1.f - .35f * current.sweep);
        const float lp = static_cast<float>(1 - std::exp(-2 * pi * cutoff / sr));
        const float gain = std::pow(10.f, current.outputDb / 20.f);
        const float engaged = modeGain * (1 - current.bypass);
        const float dryMix = .5f * (1 - current.wetOnly);
        const float wetMix = 1 - dryMix;
        std::array<float, 2> input {clean(left, -32, 32), clean(right, -32, 32)}, output {};
        for (size_t i = 0; i < channels.size(); ++i) {
            auto& c = channels[i]; const float x = input[i];
            const float hp = x - c.lastInput + inPole * c.inputHP;
            c.lastInput = x; c.inputHP = hp;
            c.pre += preLp * (hp - c.pre); // fixed input filter, outside the loop
            float pos = static_cast<float>(c.write) - delaySamples;
            if (pos < 0) pos += static_cast<float>(c.delay.size());
            const auto base = static_cast<size_t>(pos);
            const float frac = pos - static_cast<float>(base);
            const float delayed = c.delay[base] * (1 - frac) + c.delay[(base + 1) % c.delay.size()] * frac;
            c.post1 += lp * (delayed - c.post1);
            c.post2 += lp * (c.post1 - c.post2);
            const float fb = c.post2 - c.lastWet + dcPole * c.feedbackHP;
            c.lastWet = c.post2; c.feedbackHP = fb;
            // Saturation bounds the stored state even above unity loop gain.
            c.delay[c.write] = std::tanh(c.pre * engaged + fb * (1.15f * current.feedback) * (1-current.bypass));
            c.write = (c.write + 1) % c.delay.size();
            const float effect = gain * (dryMix * x + wetMix * c.post2);
            output[i] = protect(x * (1 - engaged) + effect * engaged);
        }
        return output;
    }
    // Full Width allows at most +/-1 semitone of pitch movement (2^(1/12) - 1).
    static constexpr double maxPitchRatio = 0.0594630943592953;
    // Delay in seconds for the given controls, mode and LFO phase. Public for tests.
    static double delayAt(const Controls& c, int m, double p) {
        const bool shortMode = m == 0 || m == 2;
        const bool modulating = m == 0 || m == 1;
        const double shortest = shortMode ? .00065 : .005;
        const double longest = shortMode ? .012 : .100;
        const double logRange = std::log(longest / shortest);
        const double triangle = 1 - 4 * std::abs(p - .5);
        const double rounded = -std::cos(2 * pi * p);
        const double wave = triangle + c.sweep * (rounded - triangle);
        // Width affects both available Tune span and modulation excursion.
        const double span = .25 + .75 * c.width;
        const double centre = .5 + (c.tune - .5) * span;
        // Position on a log-delay scale: clockwise Tune shortens comb modes, lengthens echo modes.
        const double v0 = shortMode ? 1 - centre : centre;
        // Full Width sweeps +/-0.285 of the log range. Depth stays available at the Tune
        // extremes: the sweep centre moves inward rather than the sweep shrinking.
        double excursion = modulating ? .285 * c.width : 0;
        double vc = std::clamp(v0, excursion, 1 - excursion);
        if (excursion > 0) {
            // Pitch deviation = |d(delay)/dt|, largest at the long end of the sweep.
            const double slope = 4 + (2 * pi - 4) * c.sweep;
            const double top = shortest * std::exp(logRange * (vc + excursion));
            const double deviation = top * logRange * excursion * slope * c.rate;
            const double limit = c.width * maxPitchRatio;
            if (deviation > limit) {
                excursion *= limit / deviation;
                vc = std::clamp(v0, excursion, 1 - excursion);
            }
        }
        const double v = std::clamp(vc + excursion * wave, 0.0, 1.0);
        return shortest * std::exp(logRange * v);
    }
    static float protect(float x) {
        if (!std::isfinite(x)) return 0;
        const float a = std::abs(x);
        return a <= .8f ? x : std::copysign(.8f + .18f * std::tanh((a - .8f) / .18f), x);
    }
private:
    static constexpr double pi = 3.14159265358979323846;
    struct Channel {
        std::vector<float> delay;
        size_t write = 0;
        float lastInput=0, inputHP=0, pre=0, post1=0, post2=0, lastWet=0, feedbackHP=0;
        void clear() {
            std::fill(delay.begin(), delay.end(), 0.f); write=0;
            lastInput=inputHP=pre=post1=post2=lastWet=feedbackHP=0;
        }
    };
    static float clean(float x, float lo, float hi) { return std::isfinite(x) ? std::clamp(x,lo,hi) : 0.f; }
    void clearLoops() { for (auto& c : channels) c.clear(); }
    void smooth(float& value, float dest) { value += paramAlpha * (dest - value); }
    static double delayFor(const Controls& c, int m, double p) { return delayAt(c, m, p); }
    std::array<Channel, 2> channels;
    Controls target {}, current {};
    double sr = 44100, phase = 0;
    float paramAlpha=0, delayAlpha=0, dcPole=0, inPole=0, preLp=1, fadeStep=0;
    float delaySeconds=.005f, modeGain=1;
    int mode=0;
};
}
