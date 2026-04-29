#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace xyzpan {

class WelcomeOverlay : public juce::Component {
public:
    WelcomeOverlay();

    std::function<void(const juce::String& key)> onSubmit;
    std::function<void()> onSkip;

    void showError(const juce::String& message);
    void showSuccess(const juce::String& message);
    void setLoading(bool loading);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::TextEditor keyInput_;
    juce::TextButton submitBtn_;
    juce::TextButton skipBtn_;
    juce::Label statusLabel_;
    bool loading_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WelcomeOverlay)
};

} // namespace xyzpan
