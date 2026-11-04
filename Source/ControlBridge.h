#pragma once
#include <JuceHeader.h>
#include <filesystem>
#if JUCE_LINUX || JUCE_MAC
#include <unistd.h>
#endif
namespace noryn
{
class ControlBridge : private juce::Timer
{
    juce::File root, instance, inbox, outbox, descriptor;
    juce::CriticalSection lock;
    std::function<juce::var(const juce::var &)> handler;
    void timerCallback() override
    {
        const juce::ScopedLock guard(lock);
        if (!handler)
            return;
        auto files = inbox.findChildFiles(juce::File::findFiles, false, "*.json");
        int processed = 0;
        for (const auto &file : files)
        {
            if (++processed > 16)
                break;
            if (file.getSize() > 131072)
            {
                file.deleteFile();
                continue;
            }
            auto request = juce::JSON::parse(file.loadFileAsString());
            juce::var response;
            try
            {
                response = handler(request);
            }
            catch (const std::exception &e)
            {
                auto *error = new juce::DynamicObject();
                error->setProperty("error", juce::String(e.what()));
                response = juce::var(error);
            }
            auto destination = outbox.getChildFile(file.getFileName());
            juce::TemporaryFile temporary(destination);
            if (temporary.getFile().replaceWithText(juce::JSON::toString(response)))
                temporary.overwriteTargetFileWithTemporary();
            file.deleteFile();
        }
    }

  public:
    const juce::String id = juce::Uuid().toString();
    ControlBridge(std::function<juce::var(const juce::var &)> callback) : handler(std::move(callback))
    {
        root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("NORYN/control");
        instance = root.getChildFile(id);
        inbox = instance.getChildFile("inbox");
        outbox = instance.getChildFile("outbox");
        inbox.createDirectory();
        outbox.createDirectory();
        std::error_code error;
        std::filesystem::permissions(root.getFullPathName().toStdString(), std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace, error);
        auto *object = new juce::DynamicObject();
        object->setProperty("id", id);
        object->setProperty("inbox", inbox.getFullPathName());
        object->setProperty("outbox", outbox.getFullPathName());
        object->setProperty("version", "0.3.0");
#if JUCE_LINUX || JUCE_MAC
        object->setProperty("pid", int(getpid()));
#endif
        descriptor = root.getChildFile(id + ".json");
        descriptor.replaceWithText(juce::JSON::toString(juce::var(object)));
        startTimer(50);
    }
    ~ControlBridge() override
    {
        stopTimer();
        const juce::ScopedLock guard(lock);
        handler = nullptr;
        descriptor.deleteFile();
        instance.deleteRecursively();
    }
};
} // namespace noryn
