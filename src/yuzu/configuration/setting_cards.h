// SPDX-License-Identifier: GPL-3.0-or-later

// Cervin: the settings pages as cards (the v0.2 mockup). Each setting sits in a card with what it
// does, what it changes in game, impact chips (+ FPS, - picture, info) and the recommended value;
// "Compact list" keeps only the names and the choices. A few choices (upscaler, output size,
// priority, DLSS model) are shown as cards to click instead of a drop-down list.

#pragma once

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include <QObject>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "common/common_types.h"

class QBoxLayout;
class QComboBox;
class QGridLayout;
class QLabel;

namespace ConfigurationShared {

enum class ChipKind {
    Plus,  ///< A gain (more FPS, fewer stutters)
    Minus, ///< A cost (less sharp, a little lag)
    Info,  ///< Good to know
};

struct GuideChip {
    ChipKind kind;
    QString text;
};

/// What a setting's card says. Every text is already translated.
struct SettingGuide {
    QString what;   ///< What it does, in one sentence
    QString effect; ///< What it changes in game
    std::vector<GuideChip> chips;
    QString reco; ///< "Recommended: ...", may be empty
};

/// The card text for a setting (its config name, e.g. "use_vsync") or for a row built by hand
/// (its "cervin_guide" property, e.g. "cervin_upscaler"). False when there is none: the card then
/// shows the row's tooltip.
bool FindGuide(std::string_view key, SettingGuide& guide);

/// Whether the cards show their explanations (false) or only names and choices (true). Saved
/// with the UI settings; every page follows a change at once.
class CardsState : public QObject {
    Q_OBJECT
public:
    static CardsState& Instance();
    bool Compact() const;
    void SetCompact(bool compact);

signals:
    void CompactChanged(bool compact);

private:
    CardsState() = default;
};

/// The page's top: a line on what the cards say, "Explanations shown" / "Compact list", and the
/// chips' legend.
QWidget* MakeCardsHeader(QWidget* parent, const QString& intro);

/// Puts every row of `layout` in a card (call once the rows are in place). A row is a
/// ConfigurationShared::Widget, a widget with a "cervin_guide" property, or a group holding one
/// (its notes and panels then sit in the same card). A widget with "cervin_section" set is opened
/// and its own rows get cards; one with "cervin_card" = "result" gets the result card's look;
/// one with "cervin_follows" set joins the card before it. A card shows while its row does.
void MakeCards(QBoxLayout* layout);

/// One section of a settings page: a title, a line on what it holds, and its settings (config
/// names such as "use_vsync", or a row's "cervin_key" / "cervin_guide" property).
struct CardSection {
    QString title;
    QString subtitle;
    std::vector<std::string_view> keys;
};

/// Sorts the cards of `layout` (call after MakeCards) into sections, in the order given, each
/// under a title that folds it on a click. Cards no section names go in a last "Other settings"
/// section. A section with no card showing hides with its title.
void MakeSections(QBoxLayout* layout, const std::vector<CardSection>& sections);

/// One choice of a ChoiceCards: a bold title and a line or two under it.
struct ChoiceText {
    QString title;
    QString text;
};

/// Shows `combo`'s entries as cards to click (the combo is hidden and keeps the value: a click
/// sets it and emits activated(), as picking it in the list does). Rows hidden in the combo's list
/// are left out; the cards follow the combo's entries, its current entry and its enabled state.
/// `describe` gives each entry's title and text (by row). The cards go under `row` in its layout
/// and show while `row` does.
QWidget* AttachChoiceCards(QComboBox* combo, QWidget* row,
                           std::function<ChoiceText(int index)> describe, int columns = 3);

/// Builds the choice cards again (after rows were hidden in the combo's list, or their texts
/// changed). Does nothing for a combo without cards.
void RefreshChoiceCards(QComboBox* combo);

/// The result card's drawing: the output picture and, inside it, the base the game draws, to
/// scale.
class SizeDiagram : public QWidget {
public:
    explicit SizeDiagram(QWidget* parent = nullptr);
    void SetSizes(u32 base_height, u32 output_height);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    u32 base = 0;
    u32 output = 0;
};

} // namespace ConfigurationShared
