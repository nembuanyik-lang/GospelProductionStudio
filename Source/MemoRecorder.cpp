#include "MemoRecorder.h"

//==============================================================================
MemoRecorder::MemoRecorder()
{
    formatManager.registerBasicFormats();
    monoBuffer.setSize (1, chunkSize);
    playBuffer.setSize (2, chunkSize);
    writerThread.startThread();
}

MemoRecorder::~MemoRecorder()
{
    stopRecording();
    stopPlayback();
    writerThread.stopThread (2000);
}

//==============================================================================
void MemoRecorder::prepare (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    juce::ignoreUnused (samplesPerBlock);
    transport.prepareToPlay (chunkSize, newSampleRate);
}

void MemoRecorder::release()
{
    transport.releaseResources();
}

void MemoRecorder::processInput (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const juce::ScopedLock sl (writerLock);

    if (activeWriter == nullptr)
        return;

    // Mix the first two input channels to mono, in chunks.
    const auto inputs = juce::jmin (2, buffer.getNumChannels());
    float peak = 0.0f;
    int done = 0;

    while (done < numSamples)
    {
        const auto n = juce::jmin (chunkSize, numSamples - done);
        auto* mono = monoBuffer.getWritePointer (0);
        juce::FloatVectorOperations::clear (mono, n);

        for (int ch = 0; ch < inputs; ++ch)
            juce::FloatVectorOperations::addWithMultiply (mono, buffer.getReadPointer (ch, startSample + done),
                                                          1.0f / (float) inputs, n);

        peak = juce::jmax (peak, monoBuffer.getMagnitude (0, 0, n));
        activeWriter->write (monoBuffer.getArrayOfReadPointers(), n);
        done += n;
    }

    samplesRecorded.fetch_add (numSamples);
    recordLevel.store (peak);
}

void MemoRecorder::renderPlayback (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    int done = 0;

    while (done < numSamples)
    {
        const auto n = juce::jmin (chunkSize, numSamples - done);
        juce::AudioSourceChannelInfo info (&playBuffer, 0, n);
        transport.getNextAudioBlock (info);     // silence when nothing is playing

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addFrom (ch, startSample + done, playBuffer, ch % playBuffer.getNumChannels(), 0, n);

        done += n;
    }
}

//==============================================================================
bool MemoRecorder::startRecording (const juce::File& file)
{
    stopRecording();
    stopPlayback();

    if (sampleRate <= 0.0)
        return false;

    file.getParentDirectory().createDirectory();
    file.deleteFile();

    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    auto* writer = wav.createWriterFor (stream.get(), sampleRate, 1, 24, {}, 0);
    if (writer == nullptr)
        return false;

    stream.release();   // the writer now owns the stream
    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer, writerThread, 32768);

    samplesRecorded.store (0);
    {
        const juce::ScopedLock sl (writerLock);
        activeWriter = threadedWriter.get();
    }
    recording.store (true);
    return true;
}

void MemoRecorder::stopRecording()
{
    {
        const juce::ScopedLock sl (writerLock);
        activeWriter = nullptr;
    }
    threadedWriter.reset();     // flushes and closes the file
    recording.store (false);
    recordLevel.store (0.0f);
}

double MemoRecorder::getRecordedSeconds() const
{
    return sampleRate > 0.0 ? (double) samplesRecorded.load() / sampleRate : 0.0;
}

//==============================================================================
bool MemoRecorder::startPlayback (const juce::File& file)
{
    stopPlayback();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    const auto fileRate = reader->sampleRate;
    auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    transport.setSource (newSource.get(), 0, nullptr, fileRate);
    readerSource = std::move (newSource);
    transport.setPosition (0.0);
    transport.start();
    return true;
}

void MemoRecorder::stopPlayback()
{
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();
}

double MemoRecorder::getFileLengthSeconds (const juce::File& file)
{
    if (! file.existsAsFile())
        return 0.0;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    return reader != nullptr && reader->sampleRate > 0
               ? (double) reader->lengthInSamples / reader->sampleRate
               : 0.0;
}
