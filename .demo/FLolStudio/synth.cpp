// synth.cpp
// Build: g++ -std=c++17 synth.cpp -O2 -o synth
// or:    clang++ -std=c++17 synth.cpp -O2 -o synth
// Run:   ./synth
// Output: output.wav (16-bit PCM mono)

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstdlib>

#define M_PI 3.14159265358979323846

struct Note {
    double pitch;      // MIDI note (0–127) or negative = frequency in Hz
    double keyTime;    // start time in beats
    double duration;   // duration in beats
    double velocity;   // 0.0 .. 1.0 amplitude scale
    std::string waveform; // "sine", "square", "saw", "triangle", "noise"

    Note(double p, double kt, double d, double vel = 1.0, std::string wf = "sine")
        : pitch(p), keyTime(kt), duration(d), velocity(vel), waveform(std::move(wf)) {}
};

// Converts MIDI note number to frequency in Hz
double midiToFreq(int midi) {
    return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
}

// Simple ADSR envelope
double adsr(double t, double noteLen, double A = 0.01, double D = 0.05, double S = 0.8, double R = 0.05) {
    if (t < 0.0) return 0.0;
    if (t < A) return t / A;                           // Attack
    t -= A;
    if (t < D) return 1.0 - (1.0 - S) * (t / D);       // Decay
    double sustainTime = noteLen - (A + D + R);
    if (t < sustainTime) return S;                     // Sustain
    double tr = t - sustainTime;
    if (tr >= R) return 0.0;                           // Done (Release)
    return S * (1.0 - tr / R);
}

// Generate waveform sample from phase (radians)
double waveformSample(const std::string& wf, double phase) {
    if (wf == "sine") {
        return std::sin(phase);
    } else if (wf == "square") {
        return (std::sin(phase) >= 0.0) ? 1.0 : -1.0;
    } else if (wf == "saw") {
        // Sawtooth wave normalized [-1,1]
        return (phase / M_PI);
    } else if (wf == "triangle") {
        double p = std::fmod(phase + M_PI, 2 * M_PI) - M_PI;
        return (2.0 / M_PI) * std::abs(p) - 1.0;
    } else if (wf == "noise") {
        return 2.0 * (double(std::rand()) / RAND_MAX) - 1.0;
    }
    return std::sin(phase);
}

// Little-endian write helper
template <typename T>
void writeLE(std::ofstream& f, T value) {
    f.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

int main() {
    // Synth parameters
    const int sampleRate = 44100;
    const double tempo = 120.0;  // beats per minute
    const int bitsPerSample = 16;
    const int numChannels = 1;
    const double masterGain = 0.9;

    // Note sequence (replace or generate programmatically)
    std::vector<Note> notes = {
        Note(69, 0.0, 1.0, 0.9, "sine"),     // A4
        Note(72, 1.0, 1.0, 0.8, "sine"),     // C5
        Note(76, 2.0, 2.0, 0.8, "sine"),     // E5
        Note(60, 0.0, 4.0, 0.6, "saw"),      // C3 chord base
        Note(64, 0.0, 4.0, 0.6, "saw"),      // E3
        Note(67, 0.0, 4.0, 0.6, "saw"),      // G3
        Note(-300.0, 0.5, 0.25, 0.5, "noise"), // Percussive noise
        Note(-600.0, 1.5, 0.25, 0.5, "noise"),
        Note(84, 3.0, 1.0, 0.7, "triangle")   // High C
    };

    double secPerBeat = 60.0 / tempo;

    // Compute total duration
    double totalSec = 0.0;
    for (const auto& n : notes) {
        double end = (n.keyTime + n.duration) * secPerBeat + 0.1;
        totalSec = std::max(totalSec, end);
    }
    int totalSamples = static_cast<int>(std::ceil(totalSec * sampleRate));

    std::vector<double> out(totalSamples, 0.0);

    // Synthesize each note
    for (const auto& note : notes) {
        double freq = (note.pitch >= 0.0)
            ? midiToFreq(static_cast<int>(std::round(note.pitch)))
            : std::fabs(note.pitch);

        double startSec = note.keyTime * secPerBeat;
        double durSec = std::max(0.001, note.duration * secPerBeat);
        int startSample = static_cast<int>(std::round(startSec * sampleRate));
        int noteSamples = static_cast<int>((durSec + 0.1) * sampleRate);

        double phase = 0.0;
        double phaseInc = 2.0 * M_PI * freq / sampleRate;

        for (int i = 0; i < noteSamples; ++i) {
            int idx = startSample + i;
            if (idx >= totalSamples) break;

            double t = i / double(sampleRate);
            double env = adsr(t, durSec);
            double s = waveformSample(note.waveform, phase) * note.velocity * env;
            out[idx] += s;

            phase += phaseInc;
            if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
        }
    }

    // Normalize and apply master gain
    double peak = 1e-9;
    for (double v : out) peak = std::max(peak, std::fabs(v));
    double norm = (peak > 0.0) ? (masterGain / peak) : 1.0;

    // Write WAV file
    std::ofstream f("output.wav", std::ios::binary);
    if (!f) {
        std::cerr << "Error: could not open output.wav for writing\n";
        return 1;
    }

    const int bytesPerSample = bitsPerSample / 8;
    uint32_t dataChunkSize = totalSamples * numChannels * bytesPerSample;
    uint32_t fmtChunkSize = 16;
    uint32_t riffChunkSize = 4 + (8 + fmtChunkSize) + (8 + dataChunkSize);

    // RIFF header
    f.write("RIFF", 4);
    writeLE<uint32_t>(f, riffChunkSize);
    f.write("WAVE", 4);

    // fmt chunk
    f.write("fmt ", 4);
    writeLE<uint32_t>(f, fmtChunkSize);
    writeLE<uint16_t>(f, 1); // PCM format
    writeLE<uint16_t>(f, static_cast<uint16_t>(numChannels));
    writeLE<uint32_t>(f, static_cast<uint32_t>(sampleRate));
    uint32_t byteRate = sampleRate * numChannels * bytesPerSample;
    writeLE<uint32_t>(f, byteRate);
    uint16_t blockAlign = numChannels * bytesPerSample;
    writeLE<uint16_t>(f, blockAlign);
    writeLE<uint16_t>(f, static_cast<uint16_t>(bitsPerSample));

    // data chunk
    f.write("data", 4);
    writeLE<uint32_t>(f, dataChunkSize);

    // Sample data
    for (int i = 0; i < totalSamples; ++i) {
        double v = out[i] * norm;
        v = std::clamp(v, -1.0, 1.0);
        int16_t sample = static_cast<int16_t>(std::round(v * 32767.0));
        writeLE<int16_t>(f, sample);
    }

    f.close();
    std::cout << "Generated output.wav (" << totalSec << " sec)\n";
    return 0;
}
