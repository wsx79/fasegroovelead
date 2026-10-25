// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// LINK: every linked Fase Groove plug-in on this computer shares its genre, swing, tempo, DAW
// sync, play button (with the clock it plays from) and, with KEY on, its key through one small
// file, Link.xml. This is the reading and writing of that file; what a genre or a key means stays
// with each product (LinkClient::Product).

#include "LinkFile.h"
#include "ProductInfo.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <cmath>

namespace FaseGroove
{
    /** What the linked plug-ins share (anchor: the system time, in high-resolution ms, at which
        the play button's clock was at beat 0). */
    struct LinkState
    {
        int genre = -2;   // -1: no genre (a random or user groove)
        float swing = -1.0f, bpm = -1.0f;
        bool sync = false, playing = false, paused = false;
        double anchor = -1.0;
        int key = -1, scale = -1;   // shared only with KEY on (-1: not shared)
        bool sameAs (const LinkState& o) const noexcept
        {
            return genre == o.genre && key == o.key && scale == o.scale && std::abs (swing - o.swing) < 0.001f && std::abs (bpm - o.bpm) < 0.01f
                && sync == o.sync && playing == o.playing && paused == o.paused && std::abs (anchor - o.anchor) < 1.0;
        }
    };

    class LinkClient
    {
    public:
        /** What a product shares and how it follows the others. */
        struct Product
        {
            virtual ~Product() = default;
            virtual LinkState localLinkState() const = 0;
            /** A genre index as Link.xml names it. */
            virtual juce::String linkGenreName (int genre) const = 0;
            /** Their genre and (with KEY on) key, by the product's own rules. */
            virtual void followLinkedGroove (const juce::XmlElement& theirs) = 0;
            /** Sets a parameter as the user would (swing, tempo, tempoSync). */
            virtual void setLinkedParameter (const char* id, float plainValue) = 0;
        };

        /** previewOn / previewFromStart / anchor: the product's play button and its clock.
            keepBeatOnTempoChange: a new tempo while playing moves the clock's start here (a product
            whose audio thread already does it passes false). */
        LinkClient (Product& p, std::atomic<bool>& previewOn, std::atomic<bool>& previewFromStart,
                    std::atomic<double>& anchor, bool keepBeatOnTempoChange)
            : product (p), playing (previewOn), fromStart (previewFromStart), clock (anchor), keepBeat (keepBeatOnTempoChange) {}

        /** Shared by every Fase Groove plug-in on this computer. */
        static juce::File file()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
                .getChildFile ("Application Support")
#endif
                .getChildFile ("Fase Groove").getChildFile ("Link.xml");
        }

        /** LINK switched on or off: nothing is taken as already seen or shared. */
        void reset() { seenText = {}; shared = {}; }
        /** What this one shares goes out again at the next update (e.g. KEY switched on). */
        void shareAgain() { shared = {}; }

        /** Message thread, a few times a second while linked: takes what another instance
            changed, or writes what this one changed. */
        void update()
        {
            const auto link = file();
            const auto me = juce::String::toHexString ((juce::pointer_sized_int) &product);

            // Someone else changed it: take what changed.
            if (link.existsAsFile())
                if (const auto text = polled.read (link); text != seenText)
                {
                    seenText = text;
                    if (auto xml = juce::parseXML (text); xml != nullptr && xml->getStringAttribute ("instance") != me)
                    {
                        product.followLinkedGroove (*xml);
                        if (xml->hasAttribute ("swing"))
                            product.setLinkedParameter ("swing", juce::jlimit (0.0f, 1.5f, (float) xml->getDoubleAttribute ("swing")));
                        if (xml->hasAttribute ("bpm"))
                            product.setLinkedParameter ("tempo", juce::jlimit (60.0f, 180.0f, (float) xml->getDoubleAttribute ("bpm")));
                        if (xml->hasAttribute ("sync"))
                            product.setLinkedParameter ("tempoSync", xml->getBoolAttribute ("sync") ? 1.0f : 0.0f);
                        if (xml->hasAttribute ("playing"))
                        {
                            const double anchor = xml->getDoubleAttribute ("anchor", -1.0);
                            if (xml->getBoolAttribute ("playing") && anchor >= 0.0)
                            {
                                clock = anchor;
                                playing = true;
                            }
                            else
                            {
                                playing = false;
                                fromStart = ! xml->getBoolAttribute ("paused");
                                clock = -1.0;
                            }
                        }
                        shared = product.localLinkState();
                        return;
                    }
                }

            // We changed it: tell the others (a random or user groove has no genre). A new tempo
            // while playing keeps the position on the clock.
            auto now = product.localLinkState();
            if (keepBeat && now.playing && now.anchor >= 0.0 && shared.bpm > 0.0f && std::abs (now.bpm - shared.bpm) >= 0.01f && ! now.sync)
            {
                const double t = juce::Time::getMillisecondCounterHiRes();
                const double beat = (t - now.anchor) * shared.bpm / 60000.0;
                clock = now.anchor = t - beat * 60000.0 / now.bpm;
            }
            if (now.sameAs (shared))
                return;

            juce::XmlElement xml ("FaseGrooveLink");
            xml.setAttribute ("from", FaseGroove::product().productName);
            xml.setAttribute ("instance", me);
            if (now.genre >= 0)
                xml.setAttribute ("genre", product.linkGenreName (now.genre));
            xml.setAttribute ("swing", now.swing);   // 0 straight, 1 a triplet shuffle
            xml.setAttribute ("bpm", now.bpm);
            xml.setAttribute ("sync", now.sync);
            xml.setAttribute ("playing", now.playing);
            xml.setAttribute ("paused", now.paused);
            xml.setAttribute ("anchor", juce::String (now.anchor, 3));
            if (now.key >= 0)
            {
                xml.setAttribute ("key", now.key);
                xml.setAttribute ("scale", now.scale);
            }
            link.getParentDirectory().createDirectory();
            if (xml.writeTo (link))
                seenText = xml.toString();   // what was just written: no need to read it back
            shared = now;
        }

    private:
        Product& product;
        std::atomic<bool>& playing;
        std::atomic<bool>& fromStart;
        std::atomic<double>& clock;
        const bool keepBeat;
        LinkState shared;        // what the link file says (or what we wrote)
        juce::String seenText;
        PolledFile polled;       // (LinkFile.h)
    };
}
