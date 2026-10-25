// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Branding.h"

#include "ProductInfo.h"

namespace
{
    const juce::Colour warm1 { 0xffffb35c };
    const juce::Colour warm2 { 0xffff7a3d };

    juce::Font boldFont (float height, float kerning = 0.0f)
    {
        juce::Font f (juce::FontOptions (height).withStyle ("Bold"));
        f.setExtraKerningFactor (kerning);
        return f;
    }

    /** The logo's parsed icon and its slanted text, kept instead of rebuilt at every paint
        (message thread only; freed when JUCE shuts down). */
    struct LogoCache : juce::DeletedAtShutdown
    {
        ~LogoCache() override { clearSingletonInstance(); }
        std::unique_ptr<juce::Drawable> icon;
        bool iconLoaded = false;
        struct Text { juce::String text; juce::Rectangle<float> area; float height = 0.0f, kerning = 0.0f; juce::Path path; };
        std::vector<Text> texts;   // a few entries: the sizes the logo is drawn at
        JUCE_DECLARE_SINGLETON_SINGLETHREADED_MINIMAL_INLINE (LogoCache)
    };

    /** Draws text with the forward slant of the FS logo. */
    void drawSlanted (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height, float kerning)
    {
        auto& texts = LogoCache::getInstance()->texts;
        for (const auto& t : texts)
            if (t.text == text && t.area == area && juce::exactlyEqual (t.height, height) && juce::exactlyEqual (t.kerning, kerning))
            {
                g.fillPath (t.path);
                return;
            }

        juce::GlyphArrangement glyphs;
        glyphs.addFittedText (boldFont (height, kerning), text, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                              juce::Justification::centredLeft, 1, 1.0f);

        juce::Path path;
        glyphs.createPath (path);
        path.applyTransform (juce::AffineTransform::shear (-0.21f, 0.0f)
                                 .translated (0.21f * area.getCentreY(), 0.0f));
        g.fillPath (path);
        if (texts.size() >= 8)
            texts.erase (texts.begin());
        texts.push_back ({ text, area, height, kerning, std::move (path) });
    }
}

namespace Branding
{
    std::unique_ptr<juce::Drawable> createIcon()
    {
        const auto& p = FaseGroove::product();
        return p.iconSvg != nullptr ? juce::Drawable::createFromImageData (p.iconSvg, (size_t) p.iconSvgSize) : nullptr;
    }

    juce::Image artistLogo()
    {
        const auto& p = FaseGroove::product();
        return p.artistLogoPng != nullptr ? juce::ImageCache::getFromMemory (p.artistLogoPng, p.artistLogoPngSize) : juce::Image();
    }

    void drawWordmark (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float h = area.getHeight();
        auto top = area.removeFromTop (h * 0.64f);
        area.removeFromTop (h * 0.06f);

        g.setGradientFill (juce::ColourGradient (warm1, top.getX(), top.getY(),
                                                 warm2, top.getX() + top.getHeight() * 3.2f, top.getBottom(), false));
        drawSlanted (g, "FASE", top, top.getHeight() * 1.18f, 0.04f);

        g.setColour (juce::Colours::white);
        drawSlanted (g, FaseGroove::product().wordmark, area, area.getHeight() * 1.05f, 0.32f);
    }

    void drawFullLogo (juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto iconArea = area.removeFromLeft (area.getHeight());
        auto* cache = LogoCache::getInstance();
        if (! cache->iconLoaded)
        {
            cache->icon = createIcon();
            cache->iconLoaded = true;
        }
        if (cache->icon != nullptr)
            cache->icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred, 1.0f);

        area.removeFromLeft (area.getHeight() * 0.18f);
        drawWordmark (g, area.reduced (0.0f, area.getHeight() * 0.08f));
    }
}
