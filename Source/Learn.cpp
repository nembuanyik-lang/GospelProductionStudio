#include "Learn.h"
#include "StudioLookAndFeel.h"
#include "ChordTrack.h"
#include "Backing.h"
#include "Harmony.h"
#include "Mixing.h"
#include "Timing.h"
#include "MasterPlugin.h"
#include "VocalTunePlugin.h"
#include "SongSafety.h"

namespace Learn
{
    //==========================================================================
    //  What a task can ask about
    //==========================================================================
    const Song* Context::practiceSong() const
    {
        const auto id = findPracticeSong (host.getLibrary());
        return id.isNotEmpty() ? host.getLibrary().find (id) : nullptr;
    }

    bool Context::practiceOpen() const
    {
        auto* s = practiceSong();
        return s != nullptr && studio().getOpenSongId() == s->id;
    }

    te::Edit* Context::edit() const
    {
        return practiceOpen() ? studio().getEdit() : nullptr;
    }

    int Context::chordCount() const
    {
        auto* e = edit();
        return e != nullptr ? (int) ChordTrack::getEvents (*e).size() : 0;
    }

    int Context::trackCount() const
    {
        auto* e = edit();
        return e != nullptr ? te::getAudioTracks (*e).size() : 0;
    }

    int Context::timelineSectionCount() const
    {
        auto* e = edit();
        return e != nullptr ? (int) Backing::getSections (*e).size() : 0;
    }

    bool Context::hasTrackNamed (const juce::String& part) const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                if (t->getName().containsIgnoreCase (part)) return true;
        return false;
    }

    bool Context::hasBand() const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                for (auto* clip : t->getClips())
                    if (clip->getName().startsWith ("Band:")) return true;
        return false;
    }

    bool Context::anyTrackMuted() const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                if (t->isMuted (false)) return true;
        return false;
    }

    int Context::clipCount() const
    {
        int n = 0;
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e)) n += t->getClips().size();
        return n;
    }

    int Context::recordingCount() const
    {
        int n = 0;
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                for (auto* clip : t->getClips())
                    if (dynamic_cast<te::WaveAudioClip*> (clip) != nullptr) ++n;
        return n;
    }

    int Context::noteCount() const
    {
        int n = 0;
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                for (auto* clip : t->getClips())
                    if (auto* mc = dynamic_cast<te::MidiClip*> (clip)) n += mc->getSequence().getNumNotes();
        return n;
    }

    bool Context::hasAudioTrack() const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                if (! StudioEngine::isInstrumentTrack (*t) && ! Mixing::isGroupBus (*t)) return true;
        return false;
    }

    bool Context::audioTrackHasInput() const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                if (! StudioEngine::isInstrumentTrack (*t) && studio().getTrackInput (*t) != nullptr) return true;
        return false;
    }

    bool Context::audioTrackArmed() const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                if (! StudioEngine::isInstrumentTrack (*t) && studio().isAudioArmed (*t)) return true;
        return false;
    }

    bool Context::hasHarmony() const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                if (Harmony::isHarmonyTrack (*t)) return true;
        return false;
    }

    bool Context::hasVocalTuning() const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                for (auto* fx : StudioEngine::getEffects (*t))
                    if (dynamic_cast<VocalTunePlugin*> (fx) != nullptr) return true;
        return false;
    }

    bool Context::hasTimingCorrection() const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                for (auto* clip : t->getClips())
                    if (auto* wc = dynamic_cast<te::WaveAudioClip*> (clip))
                        if (Timing::isCorrected (*wc)) return true;
        return false;
    }

    bool Context::hasVocalChain() const
    {
        if (auto* e = edit())
            for (auto* t : te::getAudioTracks (*e))
                if (Mixing::currentVocalChain (*t).isNotEmpty()) return true;
        return false;
    }

    int Context::busCount() const
    {
        auto* e = edit();
        return e != nullptr ? Mixing::getGroupBuses (*e).size() : 0;
    }

    bool Context::masteringOn() const
    {
        auto* e = edit();
        return e != nullptr && MasterPlugin::isActive (*e);
    }

    bool Context::hasNamedVersion() const
    {
        if (! practiceOpen()) return false;
        for (auto& v : SongSafety::list (studio().getSongFile()))
            if (v.kind == "named") return true;
        return false;
    }

    bool Context::isOnScreen (const juce::String& target) const
    {
        return findOnScreen (target) != nullptr;
    }

    //==========================================================================
    //  Progress
    //==========================================================================
    bool isDone (const juce::String& id)               { return StudioEngine::getSetting ("learn.done." + id, 0) != 0; }
    void setDone (const juce::String& id, bool done)    { StudioEngine::setSetting ("learn.done." + id, done ? 1 : 0); }

    int doneCount()
    {
        int n = 0;
        for (auto& mod : course())
            for (auto& l : mod.lessons)
                if (isDone (l.id)) ++n;
        return n;
    }

    void resetProgress()
    {
        for (auto& mod : course())
            for (auto& l : mod.lessons)
                setDone (l.id, false);
    }

    //==========================================================================
    //  The practice song
    //==========================================================================
    juce::String findPracticeSong (const SongLibrary& library)
    {
        for (auto& s : library.getSongs())
            if (s.title.trim().equalsIgnoreCase (practiceTitle))
                return s.id;
        return {};
    }

    juce::String ensurePracticeSong (SongLibrary& library)
    {
        auto id = findPracticeSong (library);
        if (id.isNotEmpty())
            return id;

        id = library.createSong();
        if (auto* existing = library.find (id))
        {
            Song s = *existing;
            s.title = practiceTitle;
            s.key = "G major";
            s.tempo = 80;
            s.theme = "Praise";
            s.notes = "Made by the Learn course for practising. You can delete it whenever you like.";
            s.sections.clear();              // lesson 2.2 teaches adding the verses and choruses
            library.update (s);
        }
        return id;
    }

    //==========================================================================
    //  Finding things on the screen
    //==========================================================================
    namespace
    {
        juce::String squash (juce::String s)
        {
            s = s.trim();
            while (s.contains ("  ")) s = s.replace ("  ", " ");
            return s;
        }

        bool matches (juce::Component& c, const juce::String& target)
        {
            if (target.startsWith ("id:"))
                return c.getComponentID() == target.substring (3);

            if (target.startsWith ("tip:"))
                if (auto* t = dynamic_cast<juce::TooltipClient*> (&c))
                    return t->getTooltip().startsWith (target.substring (4));

            if (target.startsWith ("tip:"))
                return false;

            const auto want = squash (target);
            if (auto* b = dynamic_cast<juce::Button*> (&c))
                return squash (b->getButtonText()) == want;
            if (auto* l = dynamic_cast<juce::Label*> (&c))
                return squash (l->getText()) == want;
            return false;
        }

        juce::Component* search (juce::Component& c, const juce::String& target)
        {
            if (! c.isVisible())
                return nullptr;
            if (matches (c, target) && ! c.getLocalBounds().isEmpty())
                return &c;
            // Last child first: what is drawn on top is found first
            for (int i = c.getNumChildComponents(); --i >= 0;)
                if (auto* found = search (*c.getChildComponent (i), target))
                    return found;
            return nullptr;
        }
    }

    juce::Component* findIn (juce::Component& root, const juce::String& target)
    {
        if (target.isEmpty())
            return nullptr;
        return search (root, target);
    }

    juce::Component* findOnScreen (const juce::String& target)
    {
        if (target.isEmpty())
            return nullptr;
        auto& desktop = juce::Desktop::getInstance();
        // Newest windows first (the band panel, menus) so the one on top wins
        for (int i = desktop.getNumComponents(); --i >= 0;)
            if (auto* top = desktop.getComponent (i))
                if (top->isShowing())
                    if (auto* found = search (*top, target))
                        if (found->isShowing())
                            return found;
        return nullptr;
    }

    //==========================================================================
    //  Opening what a step needs
    //==========================================================================
    void reveal (Host& host, const Step& step, std::function<void (juce::Component*)> found)
    {
        // A page inside the window (Help, Audio Setup) covers the screens: close it if the target is not on it
        const auto openers = juce::StringArray::fromTokens (step.opener, "|", {});

        if (step.target.isNotEmpty() && findOnScreen (step.target) == nullptr)
        {
            bool onScreen = false;
            for (auto& o : openers) onScreen = onScreen || findOnScreen (o) != nullptr;
            if (! onScreen && host.currentPanel().isNotEmpty() && host.currentPanel() != "Learn")
                host.closePanel();
        }
        const bool pageChange = step.where.isNotEmpty() && host.currentPage() != step.where;
        if (pageChange)
            host.showPage (step.where);

        // Then click the openers in turn, starting after the last one already showing
        auto chain = std::make_shared<std::function<void (int)>>();
        auto target = step.target;
        *chain = [chain, openers, target, found] (int i)
        {
            auto finish = [chain, found] (juce::Component* c)
            {
                found (c);
                juce::MessageManager::callAsync ([chain] { *chain = nullptr; });    // let go of the chain
            };
            if (target.isEmpty() || findOnScreen (target) != nullptr || i >= openers.size())
            {
                finish (findOnScreen (target));
                return;
            }
            // Skip openers whose window is already open (a later opener or the target is showing)
            int next = i;
            for (int j = openers.size(); --j >= i;)
                if (findOnScreen (openers[j]) != nullptr) { next = j; break; }
            if (auto* b = dynamic_cast<juce::Button*> (findOnScreen (openers[next])))
            {
                b->triggerClick();
                juce::Timer::callAfterDelay (500, [chain, next] { if (*chain) (*chain) (next + 1); });
                return;
            }
            // Not there (already open in another way, or not needed): try the next one
            juce::MessageManager::callAsync ([chain, next] { if (*chain) (*chain) (next + 1); });
        };
        juce::Timer::callAfterDelay (pageChange ? 300 : 60, [chain] { (*chain) (0); });
    }

    //==========================================================================
    //  Spotlight: a pulsing ring around the thing to click
    //==========================================================================
    namespace
    {
        class Spotlight final : public juce::Component,
                                private juce::Timer,
                                private juce::ComponentListener
        {
        public:
            Spotlight (juce::Component& t, int ms) : target (&t), lifeMs (ms)
            {
                setInterceptsMouseClicks (false, false);
                setAlwaysOnTop (true);
                auto* top = t.getTopLevelComponent();
                top->addAndMakeVisible (this);
                t.addComponentListener (this);
                t.addMouseListener (this, true);
                follow();
                startTimerHz (30);
                started = juce::Time::getMillisecondCounter();
                current = this;
            }

            ~Spotlight() override
            {
                if (target != nullptr)
                {
                    target->removeComponentListener (this);
                    target->removeMouseListener (this);
                }
                if (current == this) current = nullptr;
            }

            void paint (juce::Graphics& g) override
            {
                const auto pulse = 0.5f + 0.5f * std::sin ((float) phase);
                auto r = getLocalBounds().toFloat().reduced (margin - 4.0f);
                for (int i = 4; i >= 1; --i)
                {
                    g.setColour (StudioColours::gold.withAlpha (0.10f * pulse * (float) (5 - i)));
                    g.drawRoundedRectangle (r.expanded ((float) i * 2.0f), 8.0f + (float) i * 2.0f, 3.0f);
                }
                g.setColour (StudioColours::gold.withAlpha (0.65f + 0.35f * pulse));
                g.drawRoundedRectangle (r, 8.0f, 3.0f);
            }

            static Spotlight* current;

        private:
            static constexpr float margin = 14.0f;

            void follow()
            {
                if (target == nullptr || getParentComponent() == nullptr || ! target->isShowing())
                {
                    finish();
                    return;
                }
                auto area = getParentComponent()->getLocalArea (target, target->getLocalBounds());
                setBounds (area.expanded ((int) margin));
                toFront (false);
            }

            void timerCallback() override
            {
                phase += 0.22;
                follow();
                repaint();
                if ((int) (juce::Time::getMillisecondCounter() - started) > lifeMs)
                    finish();
            }

            void componentMovedOrResized (juce::Component&, bool, bool) override   { follow(); }
            void componentVisibilityChanged (juce::Component&) override            { follow(); }
            void componentBeingDeleted (juce::Component&) override
            {
                if (target != nullptr) target->removeComponentListener (this);
                target = nullptr;
                finish();
            }
            void mouseDown (const juce::MouseEvent&) override                      { finish(); }

            void finish()
            {
                stopTimer();
                juce::Component::SafePointer<juce::Component> self (this);
                juce::MessageManager::callAsync ([self] { delete self.getComponent(); });
            }

            juce::Component::SafePointer<juce::Component> target;
            int lifeMs;
            juce::uint32 started = 0;
            double phase = 0.0;
        };

        Spotlight* Spotlight::current = nullptr;
    }

    void spotlight (juce::Component& target, int ms)
    {
        if (Spotlight::current != nullptr)
            delete Spotlight::current;
        new Spotlight (target, ms);           // owned by itself: removed when finished
    }

    bool isSpotlightShowing()   { return Spotlight::current != nullptr; }

    //==========================================================================
    //  The course page
    //==========================================================================
    namespace
    {
        class CoursePage final : public juce::Component
        {
        public:
            CoursePage (Host& h, std::function<void (const juce::String&)> start) : host (h), onStart (std::move (start))
            {
                using namespace StudioColours;
                startButton.setColour (juce::TextButton::buttonColourId, gold);
                startButton.setColour (juce::TextButton::textColourOffId, header);
                startButton.onClick = [this] { if (onStart && selected.isNotEmpty()) onStart (selected); };
                doneButton.onClick = [this]
                {
                    setDone (selected, ! isDone (selected));
                    refresh();
                };
                resetButton.setTooltip ("Clear all the ticks and start the course again (your songs are not touched)");
                resetButton.onClick = [this]
                {
                    juce::Component::SafePointer<CoursePage> safe (this);
                    juce::AlertWindow::showAsync (juce::MessageBoxOptions{}
                        .withIconType (juce::MessageBoxIconType::NoIcon)
                        .withTitle ("Start the course again?")
                        .withMessage ("All lessons will be marked as not done. Your songs, including the Practice Song, stay as they are.")
                        .withButton ("Start again", 1).withButton ("Cancel", 0),
                        [safe] (int r) { if (r == 1 && safe != nullptr) { resetProgress(); safe->refresh(); } });
                };
                handbookButton.setTooltip ("Every lesson on one page: it opens in your web browser, where Ctrl+P prints it or saves it as a PDF");
                handbookButton.onClick = [this]
                {
                    const auto file = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                          .getChildFile ("Gospel Production Studio").getChildFile ("Handbook.html");
                    if (writeHandbook (file, JUCE_APPLICATION_VERSION_STRING) != juce::File())
                        file.startAsProcess();
                };
                for (auto* b : { &startButton, &doneButton, &resetButton, &handbookButton })
                {
                    b->setWantsKeyboardFocus (false);
                    addAndMakeVisible (b);
                }

                listViewport.setViewedComponent (&list, false);
                listViewport.setScrollBarsShown (true, false);
                addAndMakeVisible (listViewport);
                list.owner = this;

                // Start where the learner left off: the first lesson not done yet
                for (auto& mod : course())
                    for (auto& l : mod.lessons)
                        if (selected.isEmpty() && ! isDone (l.id)) selected = l.id;
                if (selected.isEmpty()) selected = course().front().lessons.front().id;
                refresh();
            }

            void refresh()
            {
                const auto* l = findLesson (selected);
                startButton.setButtonText (l != nullptr && isDone (l->id) ? "Do this lesson again" : "Start this lesson");
                doneButton.setButtonText (l != nullptr && isDone (l->id) ? "Mark as not done" : "Mark as done");
                list.layout();
                repaint();
                list.repaint();
            }

            void paint (juce::Graphics& g) override
            {
                using namespace StudioColours;
                g.fillAll (background);

                // Progress across the top
                const auto done = doneCount(), total = lessonCount();
                auto top = progressArea;
                g.setColour (text);
                g.setFont (studioFont (15.0f, true));
                g.drawText ("Learn Gospel Production Studio", top.removeFromTop (22), juce::Justification::centredLeft);
                g.setColour (textDim);
                g.setFont (studioFont (12.5f));
                g.drawText (juce::String (done) + " of " + juce::String (total) + " lessons done. Lessons use a \"Practice Song\", so your real songs are never changed.",
                            top.removeFromTop (20), juce::Justification::centredLeft);
                auto bar = top.removeFromTop (8).withTrimmedTop (2).toFloat();
                g.setColour (card);
                g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (gold);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) done / (float) juce::jmax (1, total)), 3.0f);

                // The chosen lesson
                g.setColour (card);
                g.fillRoundedRectangle (detailArea.toFloat(), 10.0f);
                g.setColour (cardBorder);
                g.drawRoundedRectangle (detailArea.toFloat().reduced (0.5f), 10.0f, 1.0f);

                const auto* l = findLesson (selected);
                if (l == nullptr) return;
                auto r = detailArea.reduced (20, 16).withTrimmedBottom (50);
                g.setColour (gold);
                g.setFont (studioFont (12.0f, true));
                g.drawText ("LESSON " + l->id + juce::String::fromUTF8 ("  \xc2\xb7  ") + "ABOUT " + juce::String (l->minutes) + " MINUTES"
                                + (isDone (l->id) ? juce::String::fromUTF8 ("  \xc2\xb7  DONE \xe2\x9c\x93") : juce::String()),
                            r.removeFromTop (18), juce::Justification::centredLeft);
                g.setColour (text);
                g.setFont (studioFont (21.0f, true));
                g.drawText (l->title, r.removeFromTop (32), juce::Justification::centredLeft, true);
                r.removeFromTop (4);

                juce::AttributedString why;
                why.append (l->why, studioFont (14.5f), text);
                juce::TextLayout whyLayout;
                whyLayout.createLayout (why, (float) r.getWidth());
                whyLayout.draw (g, r.removeFromTop ((int) std::ceil (whyLayout.getHeight())).toFloat());
                r.removeFromTop (14);

                g.setColour (textDim);
                g.setFont (studioFont (12.0f, true));
                g.drawText ("IN THIS LESSON", r.removeFromTop (18), juce::Justification::centredLeft);
                r.removeFromTop (4);
                int n = 1;
                for (auto& s : l->steps)
                {
                    if (r.getHeight() < 22) break;
                    const auto line = s.task.isNotEmpty() ? s.task : firstSentence (s.text);
                    juce::AttributedString a;
                    a.append (juce::String (n++) + ".  ", studioFont (13.5f, true), gold);
                    a.append (line, studioFont (13.5f), s.task.isNotEmpty() ? text : textDim);
                    juce::TextLayout t;
                    t.createLayout (a, (float) r.getWidth());
                    const auto h = (int) std::ceil (t.getHeight());
                    t.draw (g, r.removeFromTop (h).toFloat());
                    r.removeFromTop (6);
                }
            }

            void resized() override
            {
                auto r = getLocalBounds();
                progressArea = r.removeFromTop (54);
                r.removeFromTop (10);
                auto left = r.removeFromLeft (juce::jmin (380, r.getWidth() * 42 / 100));
                {
                    auto row = left.removeFromBottom (30);
                    resetButton.setBounds (row.removeFromLeft ((row.getWidth() - 8) / 2));
                    row.removeFromLeft (8);
                    handbookButton.setBounds (row);
                }
                left.removeFromBottom (8);
                listViewport.setBounds (left);
                list.setSize (left.getWidth() - listViewport.getScrollBarThickness(), list.getHeight());
                list.layout();
                r.removeFromLeft (14);
                detailArea = r;
                auto buttons = r.reduced (20, 16).removeFromBottom (36);
                startButton.setBounds (buttons.removeFromLeft (200));
                buttons.removeFromLeft (10);
                doneButton.setBounds (buttons.removeFromLeft (160));
            }

            static juce::String firstSentence (const juce::String& s)
            {
                const auto end = s.indexOfAnyOf (".:\n");
                return end > 0 ? s.substring (0, end + 1).trim() : s;
            }

        private:
            // The list of modules and lessons on the left
            struct LessonList final : public juce::Component
            {
                CoursePage* owner = nullptr;
                struct Row { bool header; juce::String id, label; juce::Rectangle<int> area; };
                std::vector<Row> rows;

                void layout()
                {
                    rows.clear();
                    int y = 0;
                    for (auto& mod : course())
                    {
                        rows.push_back ({ true, {}, mod.title, { 0, y, getWidth(), 34 } });
                        y += 34;
                        for (auto& l : mod.lessons)
                        {
                            rows.push_back ({ false, l.id, l.id + "  " + l.title, { 0, y, getWidth(), 30 } });
                            y += 30;
                        }
                        y += 6;
                    }
                    setSize (getWidth(), y);
                }

                void paint (juce::Graphics& g) override
                {
                    using namespace StudioColours;
                    for (auto& row : rows)
                    {
                        if (row.header)
                        {
                            g.setColour (gold);
                            g.setFont (studioFont (13.0f, true));
                            g.drawText (row.label.toUpperCase(), row.area.reduced (6, 0).withTrimmedTop (10), juce::Justification::centredLeft, true);
                            continue;
                        }
                        const bool sel = owner != nullptr && owner->selected == row.id;
                        if (sel)
                        {
                            g.setColour (gold.withAlpha (0.16f));
                            g.fillRoundedRectangle (row.area.reduced (0, 1).toFloat(), 6.0f);
                        }
                        const bool done = isDone (row.id);
                        auto r = row.area.reduced (8, 0);
                        auto mark = r.removeFromLeft (22).withSizeKeepingCentre (16, 16).toFloat();
                        if (done)
                        {
                            g.setColour (good);
                            g.fillEllipse (mark);
                            g.setColour (header);
                            juce::Path tick;
                            tick.startNewSubPath (mark.getX() + 4.0f, mark.getCentreY());
                            tick.lineTo (mark.getX() + 7.0f, mark.getBottom() - 4.5f);
                            tick.lineTo (mark.getRight() - 3.5f, mark.getY() + 4.5f);
                            g.strokePath (tick, juce::PathStrokeType (2.0f));
                        }
                        else
                        {
                            g.setColour (textDim.withAlpha (0.6f));
                            g.drawEllipse (mark.reduced (1.0f), 1.5f);
                        }
                        r.removeFromLeft (6);
                        g.setColour (sel ? gold : (done ? textDim : text));
                        g.setFont (studioFont (13.5f, sel));
                        g.drawText (row.label, r, juce::Justification::centredLeft, true);
                    }
                }

                void mouseUp (const juce::MouseEvent& e) override
                {
                    for (auto& row : rows)
                        if (! row.header && row.area.contains (e.getPosition()) && owner != nullptr)
                        {
                            owner->selected = row.id;
                            owner->refresh();
                        }
                }

                void mouseDoubleClick (const juce::MouseEvent& e) override
                {
                    mouseUp (e);
                    if (owner != nullptr && owner->onStart) owner->onStart (owner->selected);
                }
            };

            Host& host;
            std::function<void (const juce::String&)> onStart;
            juce::String selected;
            LessonList list;
            juce::Viewport listViewport;
            juce::TextButton startButton, doneButton, resetButton { "Start the course again" }, handbookButton { "Print the handbook" };
            juce::Rectangle<int> progressArea, detailArea;
        };
    }

    std::unique_ptr<juce::Component> createCoursePage (Host& host, std::function<void (const juce::String&)> onStart)
    {
        return std::make_unique<CoursePage> (host, std::move (onStart));
    }

    //==========================================================================
    //  The coach card: one step at a time, over the real screens
    //==========================================================================
    class Coach final : public juce::Component,
                        private juce::Timer
    {
    public:
        Coach (Host& h, const Lesson& l, std::function<void()> finished) : host (h), lesson (l), onFinished (std::move (finished))
        {
            using namespace StudioColours;
            setWantsKeyboardFocus (false);
            setMouseClickGrabsKeyboardFocus (false);

            showButton.setButtonText ("Show me");
            backButton.setButtonText ("Back");
            lessonsButton.setButtonText ("All lessons");
            closeButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
            closeButton.setTooltip ("Stop this lesson (you can come back to it with Learn)");
            showButton.setTooltip ("Make the button to use glow, and open its page");

            showButton.onClick    = [this] { showMe(); };
            backButton.onClick    = [this] { go (index - 1); };
            nextButton.onClick    = [this] { next(); };
            lessonsButton.onClick = [this]
            {
                finishLater();
                auto& mainWindow = host;
                juce::MessageManager::callAsync ([&mainWindow] { mainWindow.showCoursePage(); });
            };
            closeButton.onClick   = [this] { finishLater(); };
            for (auto* b : { &showButton, &backButton, &nextButton, &lessonsButton, &closeButton })
            {
                b->setWantsKeyboardFocus (false);
                b->setMouseClickGrabsKeyboardFocus (false);
                addAndMakeVisible (b);
            }

            // Lessons that use the practice song get it ready
            if (lesson.startWith.isNotEmpty())
            {
                const auto id = ensurePracticeSong (host.getLibrary());
                if (lesson.startWith == "practice:studio")
                {
                    if (host.getStudio().getOpenSongId() != id)
                        host.openSongInStudio (id);
                    else
                        host.showPage ("studio");
                }
                else
                {
                    host.showPage ("library");
                    host.selectSongInLibrary (id);
                }
            }
            go (0);
            startTimerHz (5);
        }

        ~Coach() override
        {
            if (auto* s = Spotlight::current) delete s;
        }

        juce::String lessonId() const   { return lesson.id; }
        juce::String currentMessage() const { return message; }

        void paint (juce::Graphics& g) override
        {
            using namespace StudioColours;
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 12.0f);
            g.setColour (header);
            g.fillRoundedRectangle (r, 12.0f);
            g.setColour (gold);
            g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.5f);

            auto area = getLocalBounds().reduced (pad, 10);
            auto top = area.removeFromTop (16);
            g.setColour (gold);
            g.setFont (studioFont (11.5f, true));
            g.drawText ("LESSON " + lesson.id + juce::String::fromUTF8 ("  \xc2\xb7  ") + "STEP " + juce::String (index + 1) + " OF " + juce::String ((int) lesson.steps.size()),
                        top, juce::Justification::centredLeft);
            g.setColour (text);
            g.setFont (studioFont (15.0f, true));
            g.drawText (lesson.title, area.removeFromTop (24).withTrimmedRight (30), juce::Justification::centredLeft, true);
            area.removeFromTop (4);

            body.draw (g, area.removeFromTop (bodyHeight).toFloat());

            if (step().task.isNotEmpty())
            {
                area.removeFromTop (gap);
                auto t = area.removeFromTop (taskHeight).toFloat();
                g.setColour (done ? good.withAlpha (0.14f) : gold.withAlpha (0.10f));
                g.fillRoundedRectangle (t, 8.0f);
                auto mark = t.removeFromLeft (30).withSizeKeepingCentre (18, 18);
                if (done)
                {
                    g.setColour (good);
                    g.fillEllipse (mark);
                    g.setColour (header);
                    juce::Path tick;
                    tick.startNewSubPath (mark.getX() + 4.5f, mark.getCentreY());
                    tick.lineTo (mark.getX() + 8.0f, mark.getBottom() - 5.0f);
                    tick.lineTo (mark.getRight() - 4.0f, mark.getY() + 5.0f);
                    g.strokePath (tick, juce::PathStrokeType (2.2f));
                }
                else
                {
                    g.setColour (gold);
                    g.drawEllipse (mark.reduced (1.0f), 1.6f);
                }
                taskLayout.draw (g, t.reduced (2.0f, 6.0f));
            }

            if (message.isNotEmpty())
            {
                g.setColour (warn);
                g.setFont (studioFont (12.0f));
                g.drawFittedText (message, messageArea, juce::Justification::centredLeft, 2);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (pad, 10);
            closeButton.setBounds (r.getRight() - 24, r.getY() + 12, 24, 24);
            auto buttons = r.removeFromBottom (30);
            lessonsButton.setBounds (buttons.removeFromLeft (96));
            nextButton.setBounds (buttons.removeFromRight (84));
            buttons.removeFromRight (6);
            backButton.setBounds (buttons.removeFromRight (64));
            buttons.removeFromRight (6);
            showButton.setBounds (buttons.removeFromRight (84));
            messageArea = r.removeFromBottom (28).withTrimmedBottom (4);
        }

        void mouseDown (const juce::MouseEvent& e) override   { dragger.startDraggingComponent (this, e); moved = true; }   // the learner placed it
        void mouseDrag (const juce::MouseEvent& e) override
        {
            dragger.dragComponent (this, e, nullptr);
            if (auto* p = getParentComponent())
                setTopLeftPosition (juce::jlimit (0, p->getWidth() - getWidth(), getX()), juce::jlimit (0, p->getHeight() - getHeight(), getY()));
        }

        void place()
        {
            auto area = host.coachArea();
            if (! moved)
            {
                setTopLeftPosition (area.getRight() - getWidth() - 12, area.getBottom() - getHeight() - 12);
                if (auto* t = findOnScreen (step().target))       // keep the thing to use uncovered
                    avoid (*t);
            }
            else if (auto* p = getParentComponent())
                setTopLeftPosition (juce::jlimit (0, juce::jmax (0, p->getWidth() - getWidth()), getX()),
                                    juce::jlimit (0, juce::jmax (0, p->getHeight() - getHeight()), getY()));
        }

        // Tests and the audit drive the coach directly
        int stepIndex() const           { return index; }
        bool stepDone() const           { return done; }
        void goTo (int i)               { go (i); }
        void pressNext()                { next(); }
        juce::Component* pressShowMe()  { return showMe(); }

    private:
        const Step& step() const        { return lesson.steps[(size_t) index]; }

        void go (int newIndex)
        {
            index = juce::jlimit (0, (int) lesson.steps.size() - 1, newIndex);
            done = false;
            message.clear();
            takeSnapshot();
            if (auto* s = Spotlight::current) delete s;

            using namespace StudioColours;
            const int width = 400;
            juce::AttributedString a;
            a.setLineSpacing (2.0f);
            a.append (step().text, studioFont (13.5f), text);
            body.createLayout (a, (float) (width - 2 * pad));
            bodyHeight = (int) std::ceil (body.getHeight());

            taskHeight = 0;
            if (step().task.isNotEmpty())
            {
                juce::AttributedString t;
                t.append ("Your turn: ", studioFont (13.5f, true), gold);
                t.append (step().task, studioFont (13.5f, true), text);
                taskLayout.createLayout (t, (float) (width - 2 * pad - 34));
                taskHeight = juce::jmax (34, (int) std::ceil (taskLayout.getHeight()) + 14);
            }

            const int height = 10 + 16 + 24 + 4 + bodyHeight + (taskHeight > 0 ? gap + taskHeight : 0) + 28 + 30 + 10;
            setSize (width, height);
            place();

            backButton.setEnabled (index > 0);
            showButton.setVisible (step().target.isNotEmpty() || step().where.isNotEmpty());
            const bool last = index == (int) lesson.steps.size() - 1;
            nextButton.setButtonText (last ? "Finish" : juce::String::fromUTF8 ("Next \xe2\x96\xb8"));
            refreshDone();
            repaint();
        }

        void takeSnapshot()
        {
            Context c { host, snapshot };
            snapshot = {};
            snapshot.songs = (int) host.getLibrary().getSongs().size();
            snapshot.midiMessages = host.getStudio().getMidiMessageCount();
            if (auto* s = c.practiceSong()) { snapshot.tempo = s->tempo; snapshot.key = s->key; snapshot.lyricSections = (int) s->sections.size(); }
            snapshot.chords = c.chordCount();
            snapshot.tracks = c.trackCount();
            snapshot.timelineSections = c.timelineSectionCount();
            snapshot.clips = c.clipCount();
            snapshot.recordings = c.recordingCount();
            snapshot.notes = c.noteCount();
        }

        void refreshDone()
        {
            const bool wasDone = done;
            if (! done && step().check != nullptr)
            {
                Context c { host, snapshot };
                done = step().check (c);
            }
            const bool ready = step().task.isEmpty() || done;
            using namespace StudioColours;
            nextButton.setColour (juce::TextButton::buttonColourId, ready ? gold : card);
            nextButton.setColour (juce::TextButton::textColourOffId, ready ? header : textDim);
            if (done != wasDone)
            {
                if (done) message.clear();
                if (auto* s = Spotlight::current; s != nullptr && done) delete s;
                repaint();
            }
        }

        void next()
        {
            if (step().task.isNotEmpty() && ! done && message.isEmpty())
            {
                // A gentle nudge the first time; a second click moves on anyway
                message = "Not done yet. Try it (\"Show me\" helps), or press Next again to skip it.";
                repaint();
                return;
            }
            if (index + 1 < (int) lesson.steps.size())
            {
                go (index + 1);
                return;
            }
            setDone (lesson.id, true);
            finishLater();
            showFinished();
        }

        void showFinished()
        {
            const Lesson* following = nullptr;
            bool passed = false;
            for (auto& mod : course())
                for (auto& l : mod.lessons)
                {
                    if (passed && following == nullptr) following = &l;
                    if (l.id == lesson.id) passed = true;
                }
            auto& h = host;
            const juce::String nextId = following != nullptr ? following->id : juce::String();
            auto options = juce::MessageBoxOptions{}
                .withIconType (juce::MessageBoxIconType::NoIcon)
                .withTitle ("Lesson " + lesson.id + " done")
                .withMessage ("Well done! \"" + lesson.title + "\" is ticked in the course."
                              + (following != nullptr ? "\n\nNext: " + following->id + " " + following->title + "." : juce::String ("\n\nThat is the end of the course for now.")));
            if (following != nullptr)
                options = options.withButton ("Next lesson", 2);
            options = options.withButton ("All lessons", 1).withButton ("Close", 0);
            juce::AlertWindow::showAsync (options, [&h, nextId] (int r)
            {
                if (r == 2) h.startLesson (nextId);
                if (r == 1) h.showCoursePage();
            });
        }

        juce::Component* showMe()
        {
            message.clear();
            repaint();
            if (auto* found = findOnScreen (step().target); found != nullptr && (step().where.isEmpty() || host.currentPage() == step().where))
            {
                spotlight (*found);
                if (! moved) avoid (*found);
                return found;
            }
            juce::Component::SafePointer<Coach> safe (this);
            const bool hasTarget = step().target.isNotEmpty();
            reveal (host, step(), [safe, hasTarget] (juce::Component* found)
            {
                if (safe == nullptr) return;
                if (found != nullptr)
                {
                    spotlight (*found);
                    if (! safe->moved) safe->avoid (*found);
                }
                else if (hasTarget)
                {
                    safe->message = "It is not on the screen yet. Follow the step before this one first (Back), then try again.";
                    safe->repaint();
                }
            });
            return nullptr;
        }

        // Move out of the way of the thing that glows
        void avoid (juce::Component& target)
        {
            auto* p = getParentComponent();
            if (p == nullptr || target.getTopLevelComponent() != getTopLevelComponent()) return;
            const auto t = p->getLocalArea (&target, target.getLocalBounds());
            if (! getBounds().expanded (10).intersects (t)) return;
            auto area = host.coachArea();
            const bool goLeft = t.getCentreX() > area.getCentreX();
            const auto x = goLeft ? area.getX() + 12 : area.getRight() - getWidth() - 12;
            auto y = area.getBottom() - getHeight() - 12;
            if (t.getBottom() > y && t.getY() < y + getHeight() && ((goLeft && t.getX() < x + getWidth()) || (! goLeft && t.getRight() > x)))
                y = area.getY() + 12;
            setTopLeftPosition (x, y);
        }

        void timerCallback() override
        {
            refreshDone();
            toFront (false);
        }

        void finishLater()
        {
            stopTimer();
            if (auto* s = Spotlight::current) delete s;
            setVisible (false);
            if (onFinished) juce::MessageManager::callAsync (onFinished);
        }

        Host& host;
        const Lesson& lesson;
        std::function<void()> onFinished;          // the main window removes the card
        int index = 0;
        bool done = false, moved = false;
        Snapshot snapshot;
        juce::String message;
        juce::TextLayout body, taskLayout;
        int bodyHeight = 0, taskHeight = 0;
        static constexpr int pad = 16, gap = 10;
        juce::Rectangle<int> messageArea;
        juce::ComponentDragger dragger;
        juce::TextButton showButton, backButton, nextButton, lessonsButton, closeButton;
    };

    std::unique_ptr<juce::Component> createCoach (Host& host, const juce::String& lessonId, std::function<void()> onFinished)
    {
        if (auto* l = findLesson (lessonId))
            return std::make_unique<Coach> (host, *l, std::move (onFinished));
        return nullptr;
    }

    void placeCoach (juce::Component& c)
    {
        if (auto* coach = dynamic_cast<Coach*> (&c)) { coach->place(); coach->toFront (false); }
    }

    CoachState coachState (juce::Component& c)
    {
        if (auto* coach = dynamic_cast<Coach*> (&c))
            return { coach->lessonId(), coach->stepIndex(), coach->stepDone(), coach->currentMessage() };
        return {};
    }

    void coachGoTo (juce::Component& c, int step)           { if (auto* coach = dynamic_cast<Coach*> (&c)) coach->goTo (step); }
    void coachNext (juce::Component& c)                     { if (auto* coach = dynamic_cast<Coach*> (&c)) coach->pressNext(); }
    juce::Component* coachShowMe (juce::Component& c)       { if (auto* coach = dynamic_cast<Coach*> (&c)) return coach->pressShowMe(); return nullptr; }
}
