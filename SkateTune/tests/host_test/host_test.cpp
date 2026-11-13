// Dev-only tool (not shipped): loads the just-built SkateTune.vst3 the same
// way a real DAW would (JUCE's own plugin-hosting API), runs a detuned test
// tone through it and checks the output is sane, then renders the editor
// off-screen to a PNG so the GUI can be reviewed without a real DAW.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <cmath>
#include <vector>

using namespace juce;

static std::vector<float> makeSine (double freqHz, double sr, double seconds, float amp)
{
    size_t n = (size_t) (seconds * sr);
    std::vector<float> v (n);
    for (size_t i = 0; i < n; ++i)
        v[i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (double) i / sr);
    return v;
}

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: SkateTuneHostTest <path-to.vst3> [snapshot-out.png]\n";
        return 1;
    }

    const juce::String vst3Path (argv[1]);
    const juce::String snapshotPath (argc > 2 ? argv[2] : "");

    juce::ScopedJuceInitialiser_GUI juceInit;

    AudioPluginFormatManager formatManager;
    addDefaultFormatsToManager (formatManager);

    AudioPluginFormat* vst3Format = nullptr;
    for (auto* format : formatManager.getFormats())
        if (format->getName() == "VST3")
            vst3Format = format;

    if (vst3Format == nullptr)
    {
        std::cerr << "FAIL: VST3 format not available in this JUCE build\n";
        return 1;
    }

    OwnedArray<PluginDescription> found;
    vst3Format->findAllTypesForFile (found, vst3Path);

    if (found.isEmpty())
    {
        std::cerr << "FAIL: no VST3 plugin types found in " << vst3Path << "\n";
        return 1;
    }

    std::cout << "Found " << found.size() << " plugin description(s).\n";
    PluginDescription desc = *found[0];
    std::cout << "  Name=" << desc.name << " manufacturer=" << desc.manufacturerName
               << " numInputs=" << desc.numInputChannels << " numOutputs=" << desc.numOutputChannels << "\n";

    std::unique_ptr<AudioPluginInstance> instance;
    String errorMessage;
    bool finished = false;

    constexpr double sr = 44100.0;
    constexpr int blockSize = 512;

    vst3Format->createPluginInstanceAsync (desc, sr, blockSize,
        [&] (std::unique_ptr<AudioPluginInstance> created, const String& err)
        {
            instance = std::move (created);
            errorMessage = err;
            finished = true;
        });

    const uint32 startTime = Time::getMillisecondCounter();
    while (! finished && Time::getMillisecondCounter() - startTime < 10000)
        MessageManager::getInstance()->runDispatchLoopUntil (50);

    if (instance == nullptr)
    {
        std::cerr << "FAIL: createPluginInstanceAsync failed: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "Instance created OK: " << instance->getName() << "\n";

    instance->prepareToPlay (sr, blockSize);

    std::cout << "Parameters:\n";
    for (auto* param : instance->getParameters())
        std::cout << "  " << param->getName (64) << " = " << param->getText (param->getValue(), 64) << "\n";

    auto sine = makeSine (450.0, sr, 1.2, 0.4f); // A4 (440Hz) but 450Hz: ~39 cents sharp, should get pulled toward A
    AudioBuffer<float> buffer (2, blockSize);
    MidiBuffer midi;

    bool anyNaN = false, anyHuge = false;
    float maxAbsOut = 0.0f;
    double sumAbsOut = 0.0;
    int64_t totalSamples = 0;

    size_t pos = 0;
    while (pos + (size_t) blockSize <= sine.size())
    {
        buffer.clear();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < blockSize; ++i)
                w[i] = sine[pos + (size_t) i];
        }

        instance->processBlock (buffer, midi);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* r = buffer.getReadPointer (ch);
            for (int i = 0; i < blockSize; ++i)
            {
                const float v = r[i];
                if (std::isnan (v) || std::isinf (v)) anyNaN = true;
                if (std::abs (v) > 10.0f) anyHuge = true;
                maxAbsOut = std::max (maxAbsOut, std::abs (v));
                sumAbsOut += (double) std::abs (v);
                ++totalSamples;
            }
        }
        pos += (size_t) blockSize;
    }

    std::cout << "Processed " << totalSamples << " output samples.\n";
    std::cout << "  maxAbsOut="  << maxAbsOut << "  meanAbsOut=" << (sumAbsOut / (double) totalSamples) << "\n";
    std::cout << "  anyNaN/Inf=" << (anyNaN ? "YES (FAIL)" : "no") << "\n";
    std::cout << "  anyHuge="    << (anyHuge ? "YES (FAIL)" : "no") << "\n";

    const bool audioOk = ! anyNaN && ! anyHuge && maxAbsOut > 0.001f;

    bool snapshotOk = true;
    if (snapshotPath.isNotEmpty())
    {
        if (auto* editor = instance->createEditorAndMakeActive())
        {
            editor->setSize (editor->getWidth() > 0 ? editor->getWidth() : 600,
                              editor->getHeight() > 0 ? editor->getHeight() : 460);

            juce::Image snapshot = editor->createComponentSnapshot (editor->getLocalBounds());

            juce::File outFile (snapshotPath);
            outFile.deleteFile();
            juce::PNGImageFormat png;
            juce::FileOutputStream stream (outFile);
            if (stream.openedOk() && png.writeImageToStream (snapshot, stream))
                std::cout << "Wrote editor snapshot to " << outFile.getFullPathName() << "\n";
            else
            {
                std::cout << "FAIL: could not write snapshot PNG\n";
                snapshotOk = false;
            }

            instance->editorBeingDeleted (editor);
            delete editor;
        }
        else
        {
            std::cout << "FAIL: no editor created\n";
            snapshotOk = false;
        }
    }

    const bool ok = audioOk && snapshotOk;
    std::cout << (ok ? "HOST TEST PASSED" : "HOST TEST FAILED") << "\n";
    return ok ? 0 : 1;
}
