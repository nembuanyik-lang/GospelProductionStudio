#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
//  Voice memo: records the microphone to a WAV file and plays it back.
//
//  The audio thread calls processInput() and renderPlayback() on every block;
//  the screen calls start/stop from the message thread.
//==============================================================================
class MemoRecorder final
{
public:
    MemoRecorder();
    ~MemoRecorder();

    //==========================================================================
    // Called from MainComponent's audio callbacks
    void prepare (double sampleRate, int samplesPerBlock);
    void release();
    void processInput (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void renderPlayback (juce::AudioBuffer<float>& output, int startSample, int numSamples);

    //==========================================================================
    // Called from the screen
    bool startRecording (const juce::File& file);
    void stopRecording();
    bool isRecording() const                  { return recording.load(); }
    double getRecordedSeconds() const;
    float getRecordingLevel() const           { return recordLevel.load(); }

    bool startPlayback (const juce::File& file);
    void stopPlayback();
    bool isPlaying() const                    { return transport.isPlaying(); }
    double getPlaybackPosition() const        { return transport.getCurrentPosition(); }

    double getFileLengthSeconds (const juce::File& file);

private:
    juce::TimeSliceThread writerThread { "Memo writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::CriticalSection writerLock;
    juce::AudioFormatWriter::ThreadedWriter* activeWriter = nullptr;

    std::atomic<bool>        recording        { false };
    std::atomic<juce::int64> samplesRecorded  { 0 };
    std::atomic<float>       recordLevel      { 0.0f };

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;

    juce::AudioBuffer<float> monoBuffer, playBuffer;
    double sampleRate = 0.0;

    static constexpr int chunkSize = 4096;

    JUCE_DECLARE_NON_COPYABLE (MemoRecorder)
};
