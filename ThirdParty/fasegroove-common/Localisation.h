// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// User-interface translations, shared by the Fase Groove family. The English text is the key:
// tr ("CLOSE") returns the text in the current language, or the English text itself when no
// translation exists (groove names and musical terms stay as they are).
// The chosen language is a user preference shared by every project.
//
// Texts set once on a component (a button, a tooltip, a caption, a menu's items) are "bound":
// when the language changes they are all set again, so the interface changes at once.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace Localisation
{
    enum class Language { english = 0, italian, spanish, french, german, portuguese };
    constexpr int numLanguages = 6;

    /** Native name of a language, e.g. "Italiano". */
    const char* languageName (int language) noexcept;

    Language current() noexcept;

    /** Changes the language, saves it for next time and updates every bound text. */
    void setCurrent (Language);

    /** Loads the saved language, or picks the computer's language the first time. */
    void loadSaved();

    /** Other user preferences shared by every project (same settings file as the language). */
    juce::String setting (const char* key, const juce::String& fallback = {});
    void setSetting (const char* key, const juce::var& value);

    /** Tips when the mouse rests on a control (on by default). */
    bool tipsEnabled();
    void setTipsEnabled (bool);

    /** Upper case that also handles accented letters. */
    juce::String upper (const juce::String&);

    /** Translates an English UI text into the current language. */
    juce::String tr (const juce::String& english);

    /** One row of a translation table: the English text is the key. */
    struct Entry { const char* en; const char* it; const char* es; const char* fr; const char* de; const char* pt; };
    struct Table { const Entry* rows; std::size_t size; };

    /** The product's own texts (its Source/Translations.inc, defined in Source/Product.cpp).
        They are read after the shared CommonTranslations.inc, so a product row wins. */
    Table productTranslations();

    // ---- Texts that follow the language ----
    void bindText (juce::Button&, const juce::String& english);
    void bindTip (juce::Component&, const juce::String& english);   // the component must be a SettableTooltipClient
    void bindLabel (juce::Label&, const juce::String& english, bool upperCase = true);
    /** A menu's items, by id (the selection stays). */
    void bindItems (juce::ComboBox&, const juce::StringArray& english, int firstId = 1);
    /** Anything else: called now and on every language change while the component exists. */
    void bind (juce::Component&, std::function<void()> apply);
}

using Localisation::tr;
