#include "BackgroundWork.h"
#include <juce_core/juce_core.h>

namespace BackgroundWork
{
    namespace
    {
        struct State
        {
            juce::CriticalSection lock;
            std::vector<std::weak_ptr<Token>> tokens;
            std::unique_ptr<juce::ThreadPool> pool;
            std::shared_ptr<std::atomic<bool>> stopping = std::make_shared<std::atomic<bool>> (false);
            std::shared_ptr<std::atomic<bool>> closing = std::make_shared<std::atomic<bool>> (false);
            std::atomic<int> running { 0 };
        };

        State& state()
        {
            static State s;
            return s;
        }

        juce::ThreadPool& pool()
        {
            auto& s = state();
            const juce::ScopedLock sl (s.lock);
            if (s.pool == nullptr)
                s.pool = std::make_unique<juce::ThreadPool> (juce::ThreadPoolOptions{}.withNumberOfThreads (2).withThreadName ("Studio background work"));
            return *s.pool;
        }

        // A job whose stop flag is set by stopAll() or shutdown()
        struct Job final : public juce::ThreadPoolJob
        {
            Job (const juce::String& w, Work fn, bool ask)
                : juce::ThreadPoolJob (w), work (std::move (fn)), stopFlag (std::make_shared<std::atomic<bool>> (false))
            {
                auto flag = stopFlag;
                token = busy (w, [flag] { flag->store (true); }, ask);
            }

            JobStatus runJob() override
            {
                if (! state().closing->load())
                    work (*stopFlag);
                token.reset();
                state().running.fetch_sub (1);
                return jobHasFinished;
            }

            Work work;
            std::shared_ptr<std::atomic<bool>> stopFlag;
            std::shared_ptr<Token> token;
        };
    }

    std::shared_ptr<Token> busy (const juce::String& what, std::function<void()> stop, bool ask)
    {
        auto t = std::make_shared<Token>();
        t->what = what;
        t->askBeforeClosing = ask;
        t->stop = std::move (stop);
        auto& s = state();
        const juce::ScopedLock sl (s.lock);
        s.tokens.erase (std::remove_if (s.tokens.begin(), s.tokens.end(), [] (auto& w) { return w.expired(); }), s.tokens.end());
        s.tokens.push_back (t);
        // Started while the app is closing: stop at once
        if (s.closing->load() && t->stop) t->stop();
        return t;
    }

    void run (const juce::String& what, Work work, bool ask)
    {
        auto& s = state();
        if (s.closing->load())
            return;
        s.running.fetch_add (1);
        pool().addJob (new Job (what, std::move (work), ask), true);
    }

    void onMessageThread (std::function<void()> fn)
    {
        auto closing = state().closing;
        juce::MessageManager::callAsync ([closing, fn = std::move (fn)]
        {
            if (! closing->load() && fn)
                fn();
        });
    }

    juce::StringArray whatIsBusy (bool onlyImportant)
    {
        juce::StringArray out;
        auto& s = state();
        const juce::ScopedLock sl (s.lock);
        for (auto& w : s.tokens)
            if (auto t = w.lock(); t != nullptr && (t->askBeforeClosing || ! onlyImportant))
                out.addIfNotAlreadyThere (t->what);
        return out;
    }

    bool isIdle()
    {
        return whatIsBusy (false).isEmpty() && state().running.load() == 0;
    }

    void stopAll()
    {
        std::vector<std::shared_ptr<Token>> live;
        {
            auto& s = state();
            const juce::ScopedLock sl (s.lock);
            for (auto& w : s.tokens)
                if (auto t = w.lock()) live.push_back (t);
        }
        for (auto& t : live)
            if (t->stop) t->stop();
    }

    void shutdown (int timeoutMs)
    {
        auto& s = state();
        s.closing->store (true);
        stopAll();
        std::unique_ptr<juce::ThreadPool> p;
        {
            const juce::ScopedLock sl (s.lock);
            p = std::move (s.pool);
        }
        if (p != nullptr)
            p->removeAllJobs (true, timeoutMs);
        p.reset();
    }

    bool isShuttingDown()
    {
        return state().closing->load();
    }
}
