#include "Mp3Encoder.h"

#if GPS_HAVE_LAME
 #include <lame/lame.h>
#endif

namespace Mp3Encoder
{
bool isAvailable()
{
   #if GPS_HAVE_LAME
    return true;
   #else
    return false;
   #endif
}

juce::Result encode (const juce::File& wavIn, const juce::File& mp3Out, int kbps, const Tags& tags,
                     std::function<void (float)> progress, std::function<bool()> shouldStop)
{
   #if ! GPS_HAVE_LAME
    juce::ignoreUnused (wavIn, mp3Out, kbps, tags, progress, shouldStop);
    return juce::Result::fail ("MP3 is not included in this build. Choose WAV instead.");
   #else
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (wavIn));
    if (reader == nullptr)
        return juce::Result::fail ("Could not read the rendered audio.");

    const auto sampleRate = (int) reader->sampleRate;
    const auto stereo = reader->numChannels > 1;

    auto* lame = lame_init();
    if (lame == nullptr)
        return juce::Result::fail ("The MP3 encoder could not start.");

    lame_set_in_samplerate (lame, sampleRate);
    lame_set_out_samplerate (lame, sampleRate);
    lame_set_num_channels (lame, stereo ? 2 : 1);
    lame_set_mode (lame, stereo ? JOINT_STEREO : MONO);
    lame_set_VBR (lame, vbr_off);
    lame_set_brate (lame, kbps);
    lame_set_quality (lame, 2);                 // high quality, still quick

    id3tag_init (lame);
    id3tag_add_v2 (lame);

    // Text frames: plain ASCII as it is, anything else (é, è, ç...) as UTF-16 so it shows correctly
    auto setFrame = [lame] (const char* frame, const juce::String& value)
    {
        if (value.isEmpty())
            return;
        if (value.containsOnly (" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"))
        {
            id3tag_set_fieldvalue (lame, (juce::String (frame) + "=" + value).toRawUTF8());
            return;
        }
        std::vector<unsigned short> utf16 { 0xfeff };            // byte order mark
        for (auto c : juce::String (frame) + "=" + value)
        {
            const auto cp = (juce::uint32) c;
            if (cp < 0x10000) utf16.push_back ((unsigned short) cp);
            else { const auto v = cp - 0x10000; utf16.push_back ((unsigned short) (0xd800 + (v >> 10))); utf16.push_back ((unsigned short) (0xdc00 + (v & 0x3ff))); }
        }
        utf16.push_back (0);
        id3tag_set_fieldvalue_utf16 (lame, utf16.data());
    };
    setFrame ("TIT2", tags.title);
    setFrame ("TPE1", tags.artist);
    setFrame ("TALB", tags.album);
    if (tags.year.isNotEmpty()) id3tag_set_year (lame, tags.year.toRawUTF8());
    if (tags.genre.isNotEmpty()) id3tag_set_genre (lame, tags.genre.toRawUTF8());
    setFrame ("TCOM", tags.composer);
    setFrame ("TCOP", tags.copyright);
    setFrame ("TSRC", tags.isrc);
    setFrame ("TPUB", tags.publisher);
    setFrame ("TRCK", tags.track);
    if (tags.comment.isNotEmpty() && tags.comment.containsOnly (" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"))
        id3tag_set_comment (lame, tags.comment.toRawUTF8());
    if (tags.coverArt.getSize() > 0)
        id3tag_set_albumart (lame, static_cast<const char*> (tags.coverArt.getData()), tags.coverArt.getSize());

    // The tag (with a big cover picture it can be large) is written by us at the start of the file,
    // not inside LAME's audio stream (whose buffer is too small for a 3000 x 3000 cover)
    lame_set_write_id3tag_automatic (lame, 0);

    if (lame_init_params (lame) < 0)
    {
        lame_close (lame);
        return juce::Result::fail ("The MP3 encoder does not accept this sample rate or bit rate.");
    }

    mp3Out.deleteFile();
    auto stream = std::make_unique<juce::FileOutputStream> (mp3Out);
    auto& out = *stream;
    if (! out.openedOk())
    {
        lame_close (lame);
        return juce::Result::fail ("Could not write " + mp3Out.getFullPathName());
    }

    {
        const auto tagSize = lame_get_id3v2_tag (lame, nullptr, 0);
        if (tagSize > 0)
        {
            juce::HeapBlock<unsigned char> tag ((size_t) tagSize);
            const auto written = lame_get_id3v2_tag (lame, tag.get(), tagSize);
            if (written > 0 && written <= tagSize)
                out.write (tag.get(), (size_t) written);
        }
    }

    constexpr int block = 4096;
    juce::AudioBuffer<float> buffer ((int) juce::jmax (1u, reader->numChannels), block);
    juce::HeapBlock<unsigned char> mp3 ((size_t) (block * 5 / 4 + 7200));
    auto result = juce::Result::ok();

    for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += block)
    {
        if (shouldStop && shouldStop())
        {
            result = juce::Result::fail ("Stopped.");
            break;
        }

        const auto num = (int) juce::jmin ((juce::int64) block, reader->lengthInSamples - pos);
        reader->read (&buffer, 0, num, pos, true, true);

        // LAME wants -1..1; keep the very peaks inside
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            juce::FloatVectorOperations::clip (buffer.getWritePointer (ch), buffer.getReadPointer (ch), -1.0f, 1.0f, num);

        const auto bytes = lame_encode_buffer_ieee_float (lame, buffer.getReadPointer (0),
                                                          buffer.getReadPointer (stereo ? 1 : 0),
                                                          num, mp3.get(), (int) (block * 5 / 4 + 7200));
        if (bytes < 0)
        {
            result = juce::Result::fail ("The MP3 encoder stopped with an error (" + juce::String (bytes) + ").");
            break;
        }
        out.write (mp3.get(), (size_t) bytes);

        if (progress)
            progress ((float) ((double) (pos + num) / (double) juce::jmax ((juce::int64) 1, reader->lengthInSamples)));
    }

    if (result.wasOk())
    {
        const auto bytes = lame_encode_flush (lame, mp3.get(), (int) (block * 5 / 4 + 7200));
        if (bytes > 0)
            out.write (mp3.get(), (size_t) bytes);

        unsigned char v1[128];
        const auto v1Size = lame_get_id3v1_tag (lame, v1, sizeof (v1));
        if (v1Size > 0 && v1Size <= sizeof (v1))
            out.write (v1, (size_t) v1Size);
    }

    lame_close (lame);
    out.flush();
    stream.reset();          // closed before it may be deleted (Windows cannot delete an open file)

    if (result.failed())
        mp3Out.deleteFile();
    return result;
   #endif
}
}
