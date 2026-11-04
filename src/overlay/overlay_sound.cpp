// See overlay_sound.h. Sine tones with a soft 4 ms attack and an exponential decay, mostly under -20 dBFS,
// so they sit under the game's own audio. Played on the overlay's own waveOut stream (not PlaySound, which
// is one sound per process and would cut a game's PlaySound). The device is opened on the first cue.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_sound.h"

#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace refix::overlay::sound {
namespace {

constexpr int kRate = 44100;
constexpr float kPi = 3.14159265f;
constexpr float kAttack = 0.004f;    // seconds
constexpr float kRelease = 0.003f;   // fade to silence at the end, no click
constexpr ULONGLONG kMoveGapMs = 45;

struct Tone {
    float startSec;
    float durSec;
    float freq;
    float freqEnd;   // glide target (same as freq for none)
    float gain;      // linear, 0.1 ~ -20 dBFS
};

std::vector<int16_t> Render(std::initializer_list<Tone> tones) {
    float total = 0;
    for (const Tone& t : tones) total = std::max(total, t.startSec + t.durSec);
    std::vector<float> mix((size_t)(total * kRate) + 1, 0.0f);
    for (const Tone& t : tones) {
        const size_t first = (size_t)(t.startSec * kRate);
        const size_t n = (size_t)(t.durSec * kRate);
        const float decay = std::log(316.0f) / t.durSec;   // -50 dB at the end
        float phase = 0;
        for (size_t i = 0; i < n && first + i < mix.size(); ++i) {
            const float s = (float)i / kRate;
            const float f = t.freq + (t.freqEnd - t.freq) * (s / t.durSec);
            phase += 2 * kPi * f / kRate;
            float env = std::exp(-decay * s);
            if (s < kAttack) env *= s / kAttack;
            if (t.durSec - s < kRelease) env *= (t.durSec - s) / kRelease;
            // A little second harmonic makes the sine sound less like a test tone.
            mix[first + i] += t.gain * env * (std::sin(phase) + 0.12f * std::sin(2 * phase));
        }
    }
    std::vector<int16_t> pcm(mix.size());
    for (size_t i = 0; i < mix.size(); ++i)
        pcm[i] = (int16_t)(std::clamp(mix[i], -1.0f, 1.0f) * 32767.0f);
    return pcm;
}

const std::vector<int16_t>& Samples(Cue cue) {
    static const std::vector<int16_t> open = Render({ { 0.00f, 0.16f, 660, 660, 0.10f }, { 0.06f, 0.22f, 990, 990, 0.08f } });
    static const std::vector<int16_t> close = Render({ { 0.00f, 0.12f, 880, 880, 0.08f }, { 0.05f, 0.16f, 587, 587, 0.07f } });
    static const std::vector<int16_t> select = Render({ { 0.00f, 0.05f, 1046, 880, 0.06f } });
    static const std::vector<int16_t> move = Render({ { 0.00f, 0.025f, 1568, 1568, 0.025f } });
    static const std::vector<int16_t> notify = Render({ { 0.00f, 0.35f, 784, 784, 0.09f }, { 0.09f, 0.45f, 1175, 1175, 0.07f } });
    static const std::vector<int16_t> denied = Render({ { 0.00f, 0.14f, 330, 294, 0.08f } });
    switch (cue) {
    case Cue::Open: return open;
    case Cue::Close: return close;
    case Cue::Select: return select;
    case Cue::Move: return move;
    case Cue::Notify: return notify;
    case Cue::Denied: return denied;
    }
    return select;
}

std::atomic<bool> g_enabled{true};
HWAVEOUT g_out = nullptr;
bool g_openFailed = false;
WAVEHDR g_hdr{};
bool g_hdrPrepared = false;
ULONGLONG g_lastMove = 0;

bool EnsureDevice() {
    if (g_out) return true;
    if (g_openFailed) return false;
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = kRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = kRate * 2;
    if (waveOutOpen(&g_out, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        g_out = nullptr;
        g_openFailed = true;   // no audio device: stay silent
        return false;
    }
    return true;
}

} // namespace

void SetEnabled(bool enabled) { g_enabled = enabled; }

void Play(Cue cue) {
    if (!g_enabled) return;
    if (cue == Cue::Move) {
        const ULONGLONG now = GetTickCount64();
        if (now - g_lastMove < kMoveGapMs) return;   // holding a direction must not buzz
        g_lastMove = now;
    }
    if (!EnsureDevice()) return;
    // One cue at a time on our stream: the new one replaces whatever is still fading out.
    waveOutReset(g_out);
    if (g_hdrPrepared) waveOutUnprepareHeader(g_out, &g_hdr, sizeof(g_hdr));
    const std::vector<int16_t>& pcm = Samples(cue);
    g_hdr = WAVEHDR{};
    g_hdr.lpData = (LPSTR)pcm.data();
    g_hdr.dwBufferLength = (DWORD)(pcm.size() * sizeof(int16_t));
    g_hdrPrepared = waveOutPrepareHeader(g_out, &g_hdr, sizeof(g_hdr)) == MMSYSERR_NOERROR;
    if (g_hdrPrepared) waveOutWrite(g_out, &g_hdr, sizeof(g_hdr));
}

} // namespace refix::overlay::sound
