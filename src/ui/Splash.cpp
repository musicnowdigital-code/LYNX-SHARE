#include "ui/Splash.h"

#include "core/StartupSound.h"
#include "engine/PluginHost.h"
#include "ui/I18n.h"
#include "ui/WindowOwner.h"
#include "ui/Theme.h"

namespace lynx
{
namespace
{
double clamp01 (double x) { return juce::jlimit (0.0, 1.0, x); }
double easeOutCubic (double u) { return 1.0 - std::pow (1.0 - clamp01 (u), 3.0); }

} // namespace

// --- options --------------------------------------------------------------------------------------

StartupOptions StartupOptions::load()
{
    StartupOptions options;
    juce::StringArray lines;
    lines.addLines (PluginHost::settingsFolder().getChildFile ("ui-options.txt").loadFileAsString());

    for (const auto& line : lines)
    {
        const auto key = line.upToFirstOccurrenceOf ("=", false, false).trim();
        const auto on = line.fromFirstOccurrenceOf ("=", false, false).trim() != "0";

        if (key == "splash")
            options.splash = on;
        else if (key == "startupSound")
            options.sound = on;
    }

    return options;
}

std::shared_ptr<const SampleData> makeStartupSample()
{
    auto sound = startup::makeSound (48000.0);
    auto sample = std::make_shared<SampleData>();
    sample->left = std::move (sound.left);
    sample->right = std::move (sound.right);
    sample->sampleRate = sound.sampleRate;
    return sample;
}

// --- the splash -----------------------------------------------------------------------------------

SplashScreen::SplashScreen()
    : brandLogo (logoImage())
{
    setOpaque (false);
    setSize (width, height);
    status = tr ("Starting the audio engine...");
}

SplashScreen::~SplashScreen()
{
    stopTimer();
}

void SplashScreen::showOnDesktop()
{
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        setCentrePosition (display->userArea.getCentre());

    addToDesktop (0); // no title bar, no taskbar entry
    setAlwaysOnTop (true);
    setVisible (true);
    toFront (false);
    startTimerHz (60);
}

void SplashScreen::startClock()
{
    clockStartMs = juce::Time::getMillisecondCounterHiRes();
    time = 0.0;
}

double SplashScreen::remainingDuration (double sinceProcessStart, double landing)
{
    return juce::jmax (landing, minimumDuration - juce::jmax (0.0, sinceProcessStart));
}

double SplashScreen::getElapsed() const
{
    return clockStartMs < 0.0 ? 0.0 : (juce::Time::getMillisecondCounterHiRes() - clockStartMs) / 1000.0;
}

void SplashScreen::setStatus (const juce::String& text)
{
    status = text;
    repaint();
}

void SplashScreen::finish (std::function<void()> whenGone)
{
    onGone = std::move (whenGone);
    fadeStartMs = juce::Time::getMillisecondCounterHiRes();

    if (! isTimerRunning())
        startTimerHz (60);
}

void SplashScreen::setTimeForSnapshot (double seconds)
{
    time = seconds;
    repaint();
}

void SplashScreen::timerCallback()
{
    time = getElapsed();

    if (fadeStartMs >= 0.0)
    {
        const auto f = (juce::Time::getMillisecondCounterHiRes() - fadeStartMs) / 1000.0 / fadeOutTime;
        setAlpha ((float) (1.0 - clamp01 (f)));

        if (f >= 1.0)
        {
            stopTimer();
            setVisible (false);
            removeFromDesktop();

            if (auto gone = std::move (onGone))
                juce::MessageManager::callAsync (std::move (gone)); // may delete this splash
            return;
        }
    }

    repaint();
}

void SplashScreen::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto t = time;
    const auto card = getLocalBounds().toFloat().reduced (0.5f);
    juce::Path cardShape;
    cardShape.addRoundedRectangle (card, 14.0f);

    // card: dark, a touch lighter at the top
    g.setGradientFill (juce::ColourGradient (colour::bg3, 0.0f, 0.0f, colour::bg0, 0.0f, card.getBottom(), false));
    g.fillPath (cardShape);

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (cardShape);

    // Preserve the official artwork's proportions; the app name is separate from the brand mark.
    g.setOpacity ((float) easeOutCubic (t / 0.35));
    g.drawImageWithin (brandLogo, 60, 20, getWidth() - 120, 205, juce::RectanglePlacement::centred);
    g.setOpacity (1.0f);
    g.setColour (juce::Colour (0xffd6b56a));
    g.setFont (juce::Font (juce::FontOptions (32.0f, juce::Font::bold)));
    g.drawFittedText ("Buzz Beat Studio", 24, 230, getWidth() - 48, 42, juce::Justification::centred, 1);
    g.setColour (colour::textSecondary);
    g.setFont (font (type::small));
    g.drawText (tr ("Digital audio workstation") + "  |  "
                    + tr ("Version {0}", { JUCE_APPLICATION_VERSION_STRING }),
                24, 274, getWidth() - 48, 18, juce::Justification::centred);

    // status and progress along the bottom
    {
        const auto bar = juce::Rectangle<float> (24.0f, card.getBottom() - 20.0f, card.getWidth() - 48.0f, 3.0f);
        g.setColour (colour::bg4);
        g.fillRoundedRectangle (bar, 1.5f);
        g.setColour (colour::accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) easeOutCubic (juce::jmin (1.0, t / juce::jmax (0.1, barDuration)))), 1.5f);
        g.setColour (colour::textDisabled);
        g.setFont (font (type::small));
        g.drawText (status, juce::Rectangle<float> (24.0f, bar.getY() - 24.0f, card.getWidth() - 48.0f, 18.0f), juce::Justification::centredLeft);
    }

    // hairline border on top of everything
    g.setColour (colour::borderStrong);
    g.strokePath (cardShape, juce::PathStrokeType (1.0f));
}
} // namespace lynx
