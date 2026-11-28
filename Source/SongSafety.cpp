#include "SongSafety.h"
#include "StudioLookAndFeel.h"

namespace SongSafety
{
    //==========================================================================
    //  Versions
    //==========================================================================
    static const char* separator = " ~ ";

    juce::File versionsFolder (const juce::File& editFile)
    {
        return editFile.getParentDirectory().getChildFile ("Versions");
    }

    static juce::String kindLabel (const juce::String& kind)
    {
        if (kind == "opened")   return "When the song was opened";
        if (kind == "auto")     return "During work (every 10 minutes)";
        if (kind == "closed")   return "When the song was closed";
        if (kind == "restore")  return "Before going back to an older version";
        if (kind == "rescue")   return "Before rescuing a recording";
        return "Saved by you";
    }

    juce::String Version::describe() const
    {
        const auto when = time.formatted ("%a %d %b %Y, %H:%M");
        return kind == "named" ? when + "   \"" + name + "\"" : when + "   " + kindLabel (kind);
    }

    static Version parse (const juce::File& f)
    {
        // "2026-11-28 00-31-05 ~ auto.tracktionedit" or "2026-11-28 00-31-05 ~ named ~ Before the choir.tracktionedit"
        Version v;
        v.file = f;
        const auto parts = juce::StringArray::fromTokens (f.getFileNameWithoutExtension(), separator, "");
        juce::StringArray bits;
        auto rest = f.getFileNameWithoutExtension();
        while (rest.contains (separator))
        {
            bits.add (rest.upToFirstOccurrenceOf (separator, false, false));
            rest = rest.fromFirstOccurrenceOf (separator, false, false);
        }
        bits.add (rest);
        const auto stamp = bits[0];
        const int year = stamp.substring (0, 4).getIntValue(), month = stamp.substring (5, 7).getIntValue(), day = stamp.substring (8, 10).getIntValue();
        const int hour = stamp.substring (11, 13).getIntValue(), minute = stamp.substring (14, 16).getIntValue(), second = stamp.substring (17, 19).getIntValue();
        v.time = year > 2000 ? juce::Time (year, month - 1, day, hour, minute, second, 0, true) : f.getLastModificationTime();
        v.kind = bits.size() > 1 ? bits[1] : "auto";
        v.name = bits.size() > 2 ? bits[2] : juce::String();
        juce::ignoreUnused (parts);
        return v;
    }

    std::vector<Version> list (const juce::File& editFile)
    {
        std::vector<Version> out;
        for (auto& f : versionsFolder (editFile).findChildFiles (juce::File::findFiles, false, "*.tracktionedit"))
            out.push_back (parse (f));
        std::sort (out.begin(), out.end(), [] (const Version& a, const Version& b)
        {
            return a.time != b.time ? a.time > b.time : a.file.getFileName() > b.file.getFileName();
        });
        return out;
    }

    static juce::String safeName (const juce::String& name)
    {
        return juce::File::createLegalFileName (name.replace (separator, " ").replace ("~", "-").trim()).substring (0, 60).trim();
    }

    bool snapshot (const juce::File& editFile, const juce::String& kind, const juce::String& name)
    {
        if (! editFile.existsAsFile() || ! isReadableSong (editFile))
            return false;
        const auto folder = versionsFolder (editFile);
        folder.createDirectory();

        // Nothing changed since the newest version: no copy (a named one is always kept)
        const auto existing = list (editFile);
        if (kind != "named" && ! existing.empty() && existing.front().file.hasIdenticalContentTo (editFile))
            return false;

        auto now = juce::Time::getCurrentTime();
        juce::String fileName = now.formatted ("%Y-%m-%d %H-%M-%S") + separator + kind;
        if (kind == "named" && safeName (name).isNotEmpty())
            fileName << separator << safeName (name);
        auto target = folder.getChildFile (fileName + ".tracktionedit");
        for (int i = 2; target.exists(); ++i)
            target = folder.getChildFile (fileName + " (" + juce::String (i) + ").tracktionedit");

        const auto part = target.withFileExtension (".part");
        if (! editFile.copyFileTo (part) || ! part.moveFileTo (target))
        {
            part.deleteFile();
            return false;
        }
        prune (editFile, now);
        return true;
    }

    int prune (const juce::File& editFile, juce::Time now)
    {
        int removed = 0, automatic = 0;
        std::set<juce::String> daysKept;
        for (auto& v : list (editFile))                               // newest first
        {
            if (v.kind == "named" || v.kind == "restore" || v.kind == "rescue")
                continue;                                             // kept until you delete them
            ++automatic;
            const auto day = v.time.formatted ("%Y-%m-%d");
            if (automatic <= 20)
            {
                daysKept.insert (day);
                continue;
            }
            const bool withinMonth = (now - v.time).inDays() <= 30.0;
            if (withinMonth && daysKept.insert (day).second)
                continue;                                             // the newest of that day
            if (v.file.deleteFile())
                ++removed;
        }
        return removed;
    }

    bool restore (const juce::File& editFile, const Version& v)
    {
        if (! isReadableSong (v.file))
            return false;
        snapshot (editFile, "restore");
        const auto part = editFile.getSiblingFile (editFile.getFileName() + ".part");
        if (! v.file.copyFileTo (part))
            return false;
        editFile.deleteFile();
        return part.moveFileTo (editFile);
    }

    juce::String allVersionsText (const juce::File& songFolder)
    {
        juce::String all;
        for (auto& f : songFolder.getChildFile ("Versions").findChildFiles (juce::File::findFiles, false, "*.tracktionedit"))
            all << f.loadFileAsString() << "\n";
        return all;
    }

    //==========================================================================
    //  Repairs
    //==========================================================================
    bool isReadableSong (const juce::File& f)
    {
        if (! f.existsAsFile() || f.getSize() < 20)
            return false;
        const auto xml = juce::parseXML (f);
        return xml != nullptr && xml->hasTagName ("EDIT");
    }

    Repair prepareSongFile (const juce::File& editFile)
    {
        Repair r;
        if (! editFile.getParentDirectory().isDirectory())
            return r;                                                 // a new song
        if (isReadableSong (editFile))
            return r;

        const bool existed = editFile.existsAsFile();
        const bool hadVersions = ! list (editFile).empty();
        if (! existed && ! hadVersions && ! editFile.getSiblingFile (".tmp_" + editFile.getFileNameWithoutExtension()).existsAsFile())
            return r;                                                 // really a new song

        // 1. The copy Tracktion was writing when the app stopped
        const auto temp = editFile.getSiblingFile (".tmp_" + editFile.getFileNameWithoutExtension());
        juce::File source;
        juce::String from;
        if (isReadableSong (temp))
        {
            source = temp;
            from = "the copy that was being saved";
        }
        else
        {
            // 2. The newest version that can be read
            for (auto& v : list (editFile))
                if (isReadableSong (v.file))
                {
                    source = v.file;
                    from = "the version from " + v.time.formatted ("%d %b %Y, %H:%M");
                    break;
                }
        }
        if (source == juce::File())
        {
            r.message = "The song file could not be read, and no saved version was found.";
            return r;
        }
        if (existed)
            editFile.copyFileTo (editFile.getSiblingFile (editFile.getFileNameWithoutExtension() + " (damaged).tracktionedit"));
        const auto part = editFile.getSiblingFile (editFile.getFileName() + ".part");
        if (source.copyFileTo (part))
        {
            editFile.deleteFile();
            r.changed = part.moveFileTo (editFile);
        }
        r.message = r.changed ? "The song file was damaged when the app stopped; it was repaired from " + from + "."
                              : "The song file could not be repaired.";
        return r;
    }

    bool repairWavHeader (const juce::File& f)
    {
        juce::FileInputStream in (f);
        if (! in.openedOk() || in.getTotalLength() < 44)
            return false;
        char riff[4], wave[4];
        in.read (riff, 4);
        const auto riffSize = (juce::uint32) in.readInt();
        in.read (wave, 4);
        if (memcmp (riff, "RIFF", 4) != 0 || memcmp (wave, "WAVE", 4) != 0)
            return false;
        const auto total = in.getTotalLength();

        // Find the data chunk
        juce::int64 dataHeaderPos = -1;
        juce::uint32 dataSize = 0;
        while (! in.isExhausted() && in.getPosition() + 8 <= total)
        {
            char id[4];
            const auto at = in.getPosition();
            in.read (id, 4);
            const auto size = (juce::uint32) in.readInt();
            if (memcmp (id, "data", 4) == 0) { dataHeaderPos = at; dataSize = size; break; }
            if (size == 0 || at + 8 + (juce::int64) size > total) return false;
            in.setPosition (at + 8 + (juce::int64) size + (size & 1));
        }
        if (dataHeaderPos < 0)
            return false;
        const auto realData = (juce::uint32) std::min<juce::int64> (0xffffffffLL - 64, total - (dataHeaderPos + 8));
        const auto realRiff = (juce::uint32) std::min<juce::int64> (0xffffffffLL - 8, total - 8);
        if (dataSize == realData && riffSize == realRiff)
            return false;                                                     // already fine

        juce::FileOutputStream out (f);
        if (! out.openedOk())
            return false;
        out.setPosition (4);           out.writeInt ((int) realRiff);
        out.setPosition (dataHeaderPos + 4); out.writeInt ((int) realData);
        out.flush();
        return true;
    }

    //==========================================================================
    //  Session marker
    //==========================================================================
    namespace Session
    {
        static juce::File marker;
        static std::optional<Previous> previous;
        static juce::var current;

        juce::File defaultMarkerFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("GospelProductionStudio").getChildFile ("session.json");
        }

        static juce::DynamicObject* obj()
        {
            if (current.getDynamicObject() == nullptr)
                current = new juce::DynamicObject();
            return current.getDynamicObject();
        }

        static void write()
        {
            if (marker == juce::File())
                return;
            obj()->setProperty ("lastSeen", juce::Time::currentTimeMillis());
            marker.getParentDirectory().createDirectory();
            const auto part = marker.withFileExtension (".part");
            if (part.replaceWithText (juce::JSON::toString (current)))
                part.moveFileTo (marker);
        }

        void appStarted (const juce::File& file)
        {
            marker = file != juce::File() ? file : defaultMarkerFile();
            previous.reset();
            if (marker.existsAsFile())
            {
                const auto v = juce::JSON::parse (marker.loadFileAsString());
                if (auto* o = v.getDynamicObject())
                {
                    Previous p;
                    p.songId = o->getProperty ("songId").toString();
                    p.editFile = juce::File (o->getProperty ("editFile").toString());
                    p.started = juce::Time ((juce::int64) o->getProperty ("started"));
                    p.lastSeen = juce::Time ((juce::int64) o->getProperty ("lastSeen"));
                    p.wasRecording = (bool) o->getProperty ("recording");
                    p.recordStartSeconds = (double) o->getProperty ("recordStart");
                    p.recordStartedAt = juce::Time ((juce::int64) o->getProperty ("recordStartedAt"));
                    if (auto* files = o->getProperty ("filesBefore").getArray())
                        for (auto& f : *files) p.filesBeforeRecording.add (f.toString());
                    previous = p;
                }
            }
            current = new juce::DynamicObject();
            obj()->setProperty ("started", juce::Time::currentTimeMillis());
            write();
        }

        void songOpened (const juce::String& songId, const juce::File& editFile)
        {
            obj()->setProperty ("songId", songId);
            obj()->setProperty ("editFile", editFile.getFullPathName());
            obj()->setProperty ("recording", false);
            write();
        }

        void songClosed()
        {
            obj()->removeProperty ("songId");
            obj()->removeProperty ("editFile");
            write();
        }

        static juce::Array<juce::File> audioFilesIn (const juce::File& songFolder)
        {
            juce::Array<juce::File> files;
            for (auto* pattern : { "*.wav", "*.flac", "*.aif", "*.aiff" })
            {
                files.addArray (songFolder.findChildFiles (juce::File::findFiles, false, pattern));
                files.addArray (songFolder.getChildFile ("Audio").findChildFiles (juce::File::findFiles, false, pattern));
            }
            return files;
        }

        void recordingStarted (const juce::File& editFile, double startSeconds)
        {
            juce::Array<juce::var> before;
            for (auto& f : audioFilesIn (editFile.getParentDirectory()))
                before.add (f.getFullPathName());
            obj()->setProperty ("recording", true);
            obj()->setProperty ("recordStart", startSeconds);
            obj()->setProperty ("recordStartedAt", juce::Time::currentTimeMillis());
            obj()->setProperty ("filesBefore", before);
            write();
        }

        void recordingStopped()
        {
            obj()->setProperty ("recording", false);
            obj()->removeProperty ("filesBefore");
            write();
        }

        void appClosedNormally()
        {
            if (marker != juce::File())
                marker.deleteFile();
        }

        std::optional<Previous> previousEndedUnexpectedly()   { return previous; }
        void forgetPrevious()                                  { previous.reset(); }

        juce::Array<juce::File> findCutOffRecordings (const Previous& p, te::Edit* edit)
        {
            juce::Array<juce::File> out;
            if (! p.wasRecording || p.editFile == juce::File())
                return out;
            juce::String used;
            if (edit != nullptr)
                if (auto xml = edit->state.createXml())
                    used = xml->toString();
            if (used.isEmpty())
                used = p.editFile.loadFileAsString();
            for (auto& f : audioFilesIn (p.editFile.getParentDirectory()))
            {
                if (p.filesBeforeRecording.contains (f.getFullPathName()))
                    continue;
                if (f.getLastModificationTime() < p.recordStartedAt - juce::RelativeTime::seconds (5.0))
                    continue;
                if (used.contains (f.getFileName()))
                    continue;                                          // already in the song
                out.add (f);
            }
            return out;
        }

        int rescueRecordings (te::Edit& edit, const Previous& p)
        {
            int rescued = 0;
            for (auto& f : findCutOffRecordings (p, &edit))
            {
                repairWavHeader (f);
                juce::AudioFormatManager fm;
                fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
                if (reader == nullptr || reader->lengthInSamples <= 0)
                    continue;
                const auto seconds = (double) reader->lengthInSamples / reader->sampleRate;
                reader.reset();

                auto track = edit.insertNewAudioTrack (te::TrackInsertPoint (nullptr, te::getAllTracks (edit).getLast()), nullptr);
                if (track == nullptr)
                    continue;
                track->setName ("Rescued recording" + (rescued > 0 ? " " + juce::String (rescued + 1) : juce::String()));
                const auto start = te::TimePosition::fromSeconds (juce::jmax (0.0, p.recordStartSeconds));
                track->insertWaveClip (f.getFileNameWithoutExtension(), f, { { start, start + te::TimeDuration::fromSeconds (seconds) }, {} }, false);
                ++rescued;
            }
            return rescued;
        }
    }

    //==========================================================================
    //  The Versions page
    //==========================================================================
    class VersionsPage final : public juce::Component, private juce::ListBoxModel
    {
    public:
        VersionsPage (const juce::File& f, std::function<void()> save, std::function<void (const Version&)> back)
            : editFile (f), saveNow (std::move (save)), goBackTo (std::move (back))
        {
            using namespace StudioColours;
            intro.setText ("The app keeps copies of this song: when you open it, every 10 minutes while you work, and when you close it. "
                           "Save your own before a big change. Going back keeps the song as it is now as a version too, so nothing is lost. "
                           "(Versions are small: the recordings themselves are shared.)", juce::dontSendNotification);
            intro.setFont (studioFont (14.0f));
            intro.setColour (juce::Label::textColourId, text);
            intro.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (intro);

            listBox.setModel (this);
            listBox.setRowHeight (34);
            listBox.setColour (juce::ListBox::backgroundColourId, card);
            addAndMakeVisible (listBox);

            for (auto* b : { &saveButton, &backButton, &deleteButton })
                addAndMakeVisible (*b);
            saveButton.onClick = [this] { askForName(); };
            backButton.onClick = [this] { confirmGoBack(); };
            deleteButton.onClick = [this] { deleteSelected(); };
            status.setColour (juce::Label::textColourId, gold);
            status.setFont (studioFont (13.0f, true));
            addAndMakeVisible (status);
            refresh();
        }

        void refresh()
        {
            versions = list (editFile);
            listBox.updateContent();
            listBox.repaint();
            selectedRowsChanged (listBox.getSelectedRow());
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (24, 16);
            intro.setBounds (r.removeFromTop (56));
            r.removeFromTop (6);
            auto row = r.removeFromBottom (36);
            saveButton.setBounds (row.removeFromLeft (220));
            row.removeFromLeft (10);
            backButton.setBounds (row.removeFromLeft (240));
            row.removeFromLeft (10);
            deleteButton.setBounds (row.removeFromLeft (120));
            r.removeFromBottom (6);
            status.setBounds (r.removeFromBottom (22));
            r.removeFromBottom (6);
            listBox.setBounds (r);
        }

        void paint (juce::Graphics& g) override   { g.fillAll (StudioColours::background); }

    private:
        int getNumRows() override                  { return (int) versions.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            using namespace StudioColours;
            if (row < 0 || row >= (int) versions.size()) return;
            const auto& v = versions[(size_t) row];
            if (selected) g.fillAll (gold.withAlpha (0.18f));
            g.setColour (v.kind == "named" ? gold : text);
            g.setFont (studioFont (14.0f, v.kind == "named"));
            g.drawText (v.describe(), 12, 0, w - 140, h, juce::Justification::centredLeft);
            g.setColour (textDim);
            g.setFont (studioFont (12.0f));
            const auto age = juce::Time::getCurrentTime() - v.time;
            const auto ago = age.inMinutes() < 1.0 ? juce::String ("just now")
                           : age.inHours() < 1.0 ? juce::String ((int) age.inMinutes()) + " min ago"
                           : age.inDays() < 1.0 ? juce::String ((int) age.inHours()) + " h ago"
                                                : juce::String ((int) age.inDays()) + " days ago";
            g.drawText (ago, w - 130, 0, 118, h, juce::Justification::centredRight);
        }

        void selectedRowsChanged (int row) override
        {
            const bool has = row >= 0 && row < (int) versions.size();
            backButton.setEnabled (has);
            deleteButton.setEnabled (has);
        }

        void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override   { confirmGoBack(); }

        void askForName()
        {
            auto* w = new juce::AlertWindow ("Save a version", "A name for this version, e.g. \"Before the choir\" or \"Sent to the pastor\":",
                                             juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("name", {}, "Name");
            w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            juce::Component::SafePointer<VersionsPage> safe (this);
            w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
            {
                const auto name = w->getTextEditorContents ("name").trim();
                if (r == 1 && safe != nullptr)
                {
                    if (safe->saveNow) safe->saveNow();
                    const bool ok = snapshot (safe->editFile, "named", name.isEmpty() ? juce::String ("My version") : name);
                    safe->status.setText (ok ? "Version saved." : "The version could not be saved.", juce::dontSendNotification);
                    safe->refresh();
                }
            }), true);
            if (auto* editor = w->getTextEditor ("name"))
                editor->grabKeyboardFocus();           // type the name straight away
        }

        void confirmGoBack()
        {
            const auto row = listBox.getSelectedRow();
            if (row < 0 || row >= (int) versions.size()) return;
            const auto v = versions[(size_t) row];
            juce::Component::SafePointer<VersionsPage> safe (this);
            juce::AlertWindow::showAsync (juce::MessageBoxOptions{}
                .withIconType (juce::MessageBoxIconType::QuestionIcon)
                .withTitle ("Go back to this version?")
                .withMessage ("The song will be as it was on " + v.time.formatted ("%a %d %b %Y at %H:%M")
                              + ".\n\nThe song as it is now is kept as a version first, so you can come back to it.")
                .withButton ("Go back", 1)
                .withButton ("Cancel", 0),
                [safe, v] (int ok)
                {
                    if (ok == 1 && safe != nullptr && safe->goBackTo)
                    {
                        safe->goBackTo (v);
                        if (safe != nullptr)
                        {
                            safe->status.setText ("Done: the song is back to " + v.time.formatted ("%d %b, %H:%M") + ".", juce::dontSendNotification);
                            safe->refresh();
                        }
                    }
                });
        }

        void deleteSelected()
        {
            const auto row = listBox.getSelectedRow();
            if (row < 0 || row >= (int) versions.size()) return;
            versions[(size_t) row].file.deleteFile();
            status.setText ("Version deleted.", juce::dontSendNotification);
            refresh();
        }

        juce::File editFile;
        std::function<void()> saveNow;
        std::function<void (const Version&)> goBackTo;
        std::vector<Version> versions;
        juce::Label intro, status;
        juce::ListBox listBox;
        juce::TextButton saveButton { "Save a version now..." }, backButton { "Go back to this version" }, deleteButton { "Delete" };
    };

    std::unique_ptr<juce::Component> createVersionsPage (const juce::File& editFile, std::function<void()> saveNow,
                                                         std::function<void (const Version&)> goBackTo)
    {
        return std::make_unique<VersionsPage> (editFile, std::move (saveNow), std::move (goBackTo));
    }
}
