// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/configuration/setting_cards.h"

#include <algorithm>
#include <string>

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QListView>
#include <QPainter>
#include <QPen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

#include "qt_common/config/uisettings.h"
#include "yuzu/configuration/shared_widget.h"

namespace ConfigurationShared {

namespace {

/// Hidden by a call to hide() or setVisible(false), not merely not shown yet (a page is built
/// before its dialog shows: then every child counts as hidden for isHidden()).
bool ExplicitlyHidden(const QWidget* widget) {
    return widget->testAttribute(Qt::WA_WState_Hidden) &&
           widget->testAttribute(Qt::WA_WState_ExplicitShowHide);
}

QString HeightText(u32 height) {
    if (height == 2160) {
        return QStringLiteral("4K");
    }
    if (height == 4320) {
        return QStringLiteral("8K");
    }
    return QStringLiteral("%1p").arg(height);
}

const char* ChipName(ChipKind kind) {
    switch (kind) {
    case ChipKind::Plus:
        return "CervinChipPlus";
    case ChipKind::Minus:
        return "CervinChipMinus";
    case ChipKind::Info:
        break;
    }
    return "CervinChipInfo";
}

QLabel* MakeChip(const GuideChip& chip, QWidget* parent) {
    auto* const label = new QLabel(chip.text, parent);
    label->setObjectName(QString::fromLatin1(ChipName(chip.kind)));
    label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return label;
}

/// A row without card text: its tooltip's first two lines, as the grey line it had before.
SettingGuide GuideFromTooltip(const QString& tip) {
    SettingGuide guide;
    guide.what = tip.split(QLatin1Char('\n'), Qt::SkipEmptyParts).mid(0, 2).join(QLatin1Char(' '));
    return guide;
}

/// Fills a card's explanation (what, "In game", the chips and the recommendation). Every item is
/// a widget of its own, so the box can be emptied and filled again.
void FillExplain(QWidget* box, const SettingGuide& guide) {
    auto* const layout = static_cast<QVBoxLayout*>(box->layout());
    while (QLayoutItem* const item = layout->takeAt(0)) {
        if (QWidget* const widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    if (!guide.what.isEmpty()) {
        auto* const what = new QLabel(guide.what, box);
        what->setObjectName(QStringLiteral("CervinWhat"));
        what->setWordWrap(true);
        layout->addWidget(what);
    }
    if (!guide.effect.isEmpty()) {
        auto* const effect = new QLabel(
            CardsState::tr("<b>In game:</b> %1").arg(guide.effect.toHtmlEscaped()), box);
        effect->setObjectName(QStringLiteral("CervinEffect"));
        effect->setTextFormat(Qt::RichText);
        effect->setWordWrap(true);
        layout->addWidget(effect);
    }
    if (!guide.chips.empty() || !guide.reco.isEmpty()) {
        auto* const footer = new QWidget(box);
        auto* const footer_layout = new QHBoxLayout(footer);
        footer_layout->setContentsMargins(0, 2, 0, 0);
        footer_layout->setSpacing(6);
        for (const GuideChip& chip : guide.chips) {
            footer_layout->addWidget(MakeChip(chip, footer));
        }
        footer_layout->addStretch(1);
        if (!guide.reco.isEmpty()) {
            auto* const reco = new QLabel(guide.reco, footer);
            reco->setObjectName(QStringLiteral("CervinReco"));
            reco->setWordWrap(true);
            reco->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            footer_layout->addWidget(reco);
        }
        layout->addWidget(footer);
    }
    box->setProperty("cervin_empty", layout->count() == 0);
}

/// The row a card is about: the unit itself, or the first setting row in its group.
QWidget* FindRow(QWidget* unit) {
    if (qobject_cast<Widget*>(unit) != nullptr || unit->property("cervin_guide").isValid()) {
        return unit;
    }
    if (auto* const box = qobject_cast<QBoxLayout*>(unit->layout())) {
        for (int i = 0; i < box->count(); ++i) {
            QWidget* const child = box->itemAt(i)->widget();
            if (child != nullptr && (qobject_cast<Widget*>(child) != nullptr ||
                                     child->property("cervin_guide").isValid())) {
                return child;
            }
        }
    }
    return unit;
}

std::string KeyOf(QWidget* row) {
    if (auto* const widget = qobject_cast<Widget*>(row)) {
        return widget->GetSetting().GetLabel();
    }
    return row->property("cervin_guide").toString().toStdString();
}

/// The setting's name in bold: a card's title.
void Embolden(QWidget* row) {
    QWidget* title = nullptr;
    if (auto* const widget = qobject_cast<Widget*>(row); widget != nullptr &&
                                                         widget->checkbox != nullptr) {
        title = widget->checkbox;
    }
    if (title == nullptr) {
        const auto labels = row->findChildren<QLabel*>(Qt::FindDirectChildrenOnly);
        if (!labels.isEmpty()) {
            title = labels.front();
        }
    }
    if (title != nullptr) {
        QFont font = title->font();
        font.setWeight(QFont::DemiBold);
        title->setFont(font);
    }
}

/// Keeps a card shown while its row (or, for a group, anything in it) is, and its explanation
/// shown while the row is and the list isn't compact.
class CardFollower final : public QObject {
public:
    CardFollower(QFrame* card_, QWidget* unit_, QWidget* row_, QWidget* explain_,
                 std::vector<QWidget*> contents_, bool guided_)
        : QObject(card_), card{card_}, unit{unit_}, row{row_}, explain{explain_},
          contents{std::move(contents_)}, guided{guided_} {}

    void Update() {
        bool shown = !ExplicitlyHidden(unit);
        if (shown && unit != row) {
            shown = std::ranges::any_of(contents,
                                        [](const QWidget* child) { return !ExplicitlyHidden(child); });
        }
        if (shown == ExplicitlyHidden(card)) {
            card->setVisible(shown);
        }
        const bool explained = !CardsState::Instance().Compact() && !ExplicitlyHidden(row) &&
                               !explain->property("cervin_empty").toBool();
        explain->setVisible(explained);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        switch (event->type()) {
        case QEvent::ShowToParent:
        case QEvent::HideToParent:
            Update();
            break;
        case QEvent::ToolTipChange:
            if (watched == row && !guided) {
                FillExplain(explain, GuideFromTooltip(row->toolTip()));
                Update();
            }
            break;
        default:
            break;
        }
        return false;
    }

private:
    QFrame* card;
    QWidget* unit;
    QWidget* row;
    QWidget* explain;
    std::vector<QWidget*> contents;
    bool guided;
};

/// One card of a ChoiceCards: a bold title and its text, the whole card clickable.
class ChoiceCard final : public QPushButton {
public:
    ChoiceCard(const ChoiceText& choice, QWidget* parent) : QPushButton(parent) {
        setObjectName(QStringLiteral("CervinChoice"));
        setCheckable(true);
        setAutoDefault(false);
        setDefault(false);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* const box = new QVBoxLayout(this);
        box->setContentsMargins(12, 9, 12, 10);
        box->setSpacing(3);
        auto* const title = new QLabel(choice.title, this);
        title->setObjectName(QStringLiteral("CervinChoiceTitle"));
        QFont bold = title->font();
        bold.setWeight(QFont::DemiBold);
        title->setFont(bold);
        box->addWidget(title);
        if (!choice.text.isEmpty()) {
            auto* const body = new QLabel(choice.text, this);
            body->setObjectName(QStringLiteral("CervinChoiceText"));
            body->setWordWrap(true);
            box->addWidget(body);
        }
        box->addStretch(1);
        for (QLabel* const label : findChildren<QLabel*>()) {
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
    }

    QSize sizeHint() const override {
        return layout()->sizeHint();
    }
    QSize minimumSizeHint() const override {
        return layout()->minimumSize();
    }
    bool hasHeightForWidth() const override {
        return layout()->hasHeightForWidth();
    }
    int heightForWidth(int width) const override {
        return layout()->totalHeightForWidth(width);
    }
};

/// A combo's entries as cards (AttachChoiceCards).
class ChoiceCards final : public QWidget {
public:
    ChoiceCards(QComboBox* combo_, QWidget* row_, std::function<ChoiceText(int)> describe_,
                int columns_, QWidget* parent)
        : QWidget(parent), combo{combo_}, row{row_}, describe{std::move(describe_)},
          columns{std::max(1, columns_)} {
        setObjectName(QStringLiteral("CervinChoices"));
        grid = new QGridLayout(this);
        grid->setContentsMargins(0, 4, 0, 2);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(8);
        group = new QButtonGroup(this);
        group->setExclusive(true);
        connect(group, &QButtonGroup::idClicked, this, [this](int index) {
            if (index >= 0 && index != combo->currentIndex()) {
                combo->setCurrentIndex(index);
                emit combo->activated(index);
            }
            Sync();
        });
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int) { Sync(); });
        QAbstractItemModel* const model = combo->model();
        connect(model, &QAbstractItemModel::rowsInserted, this, [this] { ScheduleRebuild(); });
        connect(model, &QAbstractItemModel::rowsRemoved, this, [this] { ScheduleRebuild(); });
        connect(model, &QAbstractItemModel::modelReset, this, [this] { ScheduleRebuild(); });
        connect(model, &QAbstractItemModel::dataChanged, this, [this] { ScheduleRebuild(); });
        connect(model, &QAbstractItemModel::layoutChanged, this, [this] { ScheduleRebuild(); });
        combo->installEventFilter(this);
        row->installEventFilter(this);
        if (ExplicitlyHidden(row)) {
            hide();
        }
        Rebuild();
    }

    void ScheduleRebuild() {
        if (pending) {
            return;
        }
        pending = true;
        QTimer::singleShot(0, this, [this] {
            pending = false;
            Rebuild();
        });
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == combo && event->type() == QEvent::EnabledChange) {
            Sync();
        } else if (watched == row && (event->type() == QEvent::ShowToParent ||
                                      event->type() == QEvent::HideToParent)) {
            setVisible(!ExplicitlyHidden(row));
        }
        return false;
    }

private:
    void Rebuild() {
        for (QAbstractButton* const button : group->buttons()) {
            group->removeButton(button);
            button->hide();
            button->deleteLater();
        }
        while (QLayoutItem* const item = grid->takeAt(0)) {
            delete item;
        }
        for (int column = 0; column < grid->columnCount(); ++column) {
            grid->setColumnStretch(column, 0);
        }
        const auto* const view = qobject_cast<QListView*>(combo->view());
        std::vector<int> shown;
        for (int index = 0; index < combo->count(); ++index) {
            if (view == nullptr || !view->isRowHidden(index)) {
                shown.push_back(index);
            }
        }
        const int count = static_cast<int>(shown.size());
        const int used = std::max(1, std::min(columns, count));
        for (int i = 0; i < count; ++i) {
            auto* const card = new ChoiceCard(describe(shown[i]), this);
            grid->addWidget(card, i / used, i % used);
            group->addButton(card, shown[i]);
        }
        for (int column = 0; column < used; ++column) {
            grid->setColumnStretch(column, 1);
        }
        Sync();
    }

    void Sync() {
        const bool enabled = combo->isEnabled();
        for (QAbstractButton* const button : group->buttons()) {
            const QSignalBlocker block(button);
            button->setChecked(group->id(button) == combo->currentIndex());
            button->setEnabled(enabled);
        }
    }

    QComboBox* combo;
    QWidget* row;
    std::function<ChoiceText(int)> describe;
    int columns;
    QGridLayout* grid = nullptr;
    QButtonGroup* group = nullptr;
    bool pending = false;
};

constexpr const char* ChoiceCardsProperty = "cervin_choice_cards";

} // Anonymous namespace

CardsState& CardsState::Instance() {
    static CardsState* const state = new CardsState;
    return *state;
}

bool CardsState::Compact() const {
    return UISettings::values.settings_compact.GetValue();
}

void CardsState::SetCompact(bool compact) {
    if (compact == Compact()) {
        return;
    }
    UISettings::values.settings_compact.SetValue(compact);
    emit CompactChanged(compact);
}

QWidget* MakeCardsHeader(QWidget* parent, const QString& intro) {
    auto* const header = new QWidget(parent);
    header->setObjectName(QStringLiteral("CervinCardsHeader"));
    auto* const layout = new QVBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(8);

    auto* const top = new QHBoxLayout;
    top->setSpacing(6);
    auto* const text = new QLabel(intro, header);
    text->setObjectName(QStringLiteral("CervinIntro"));
    text->setWordWrap(true);
    top->addWidget(text, 1);
    auto* const shown = new QPushButton(CardsState::tr("Explanations shown"), header);
    auto* const compact = new QPushButton(CardsState::tr("Compact list"), header);
    auto* const buttons = new QButtonGroup(header);
    buttons->setExclusive(true);
    for (QPushButton* const button : {shown, compact}) {
        button->setObjectName(QStringLiteral("CervinSegment"));
        button->setCheckable(true);
        button->setAutoDefault(false);
        button->setCursor(Qt::PointingHandCursor);
        buttons->addButton(button);
        top->addWidget(button, 0, Qt::AlignBottom);
    }
    CardsState& state = CardsState::Instance();
    shown->setChecked(!state.Compact());
    compact->setChecked(state.Compact());
    QObject::connect(compact, &QPushButton::toggled, header,
                     [](bool on) { CardsState::Instance().SetCompact(on); });
    layout->addLayout(top);

    auto* const legend = new QWidget(header);
    auto* const legend_layout = new QHBoxLayout(legend);
    legend_layout->setContentsMargins(0, 0, 0, 0);
    legend_layout->setSpacing(6);
    const auto add = [&](ChipKind kind, const QString& chip, const QString& meaning) {
        legend_layout->addWidget(MakeChip(GuideChip{kind, chip}, legend));
        auto* const label = new QLabel(meaning, legend);
        label->setObjectName(QStringLiteral("CervinLegend"));
        legend_layout->addWidget(label);
        legend_layout->addSpacing(10);
    };
    add(ChipKind::Plus, CardsState::tr("+ FPS"), CardsState::tr("a gain"));
    add(ChipKind::Minus, CardsState::tr("− picture"), CardsState::tr("a cost"));
    add(ChipKind::Info, CardsState::tr("info"), CardsState::tr("good to know"));
    legend_layout->addStretch(1);
    legend->setVisible(!state.Compact());
    layout->addWidget(legend);

    QObject::connect(&state, &CardsState::CompactChanged, header,
                     [shown, compact, legend](bool on) {
                         const QSignalBlocker block_shown(shown);
                         const QSignalBlocker block_compact(compact);
                         shown->setChecked(!on);
                         compact->setChecked(on);
                         legend->setVisible(!on);
                     });
    return header;
}

void MakeCards(QBoxLayout* layout) {
    if (layout == nullptr) {
        return;
    }
    // The widgets first: wrapping them changes the layout.
    std::vector<QWidget*> units;
    for (int i = 0; i < layout->count(); ++i) {
        if (QWidget* const widget = layout->itemAt(i)->widget()) {
            units.push_back(widget);
        }
    }
    QVBoxLayout* last_card = nullptr;
    for (QWidget* const unit : units) {
        if (unit->property("cervin_section").toBool()) {
            MakeCards(qobject_cast<QBoxLayout*>(unit->layout()));
            last_card = nullptr;
            continue;
        }
        if (unit->property("cervin_follows").toBool() && last_card != nullptr) {
            layout->removeWidget(unit);
            last_card->addWidget(unit);
            continue;
        }

        auto* const card = new QFrame(unit->parentWidget());
        card->setObjectName(QStringLiteral("CervinCard"));
        if (unit->property("cervin_card").toString() == QStringLiteral("result")) {
            card->setProperty("result", true);
        }
        auto* const card_layout = new QVBoxLayout(card);
        card_layout->setContentsMargins(14, 10, 14, 12);
        card_layout->setSpacing(6);
        delete layout->replaceWidget(unit, card);
        card_layout->addWidget(unit);
        last_card = card_layout;

        QWidget* const row = FindRow(unit);
        SettingGuide guide;
        const bool guided = FindGuide(KeyOf(row), guide);
        if (!guided) {
            guide = GuideFromTooltip(row->toolTip());
        }
        if (row != unit || qobject_cast<Widget*>(row) != nullptr ||
            row->property("cervin_guide").isValid()) {
            Embolden(row);
        }

        auto* const explain = new QWidget(card);
        explain->setObjectName(QStringLiteral("CervinExplain"));
        auto* const explain_layout = new QVBoxLayout(explain);
        explain_layout->setContentsMargins(0, 0, 0, 0);
        explain_layout->setSpacing(4);
        FillExplain(explain, guide);
        // Under the row, inside its group when the row is in one (above the group's notes).
        auto* const unit_box = qobject_cast<QBoxLayout*>(unit->layout());
        if (row != unit && unit_box != nullptr && unit_box->indexOf(row) >= 0) {
            explain->setParent(unit);
            unit_box->insertWidget(unit_box->indexOf(row) + 1, explain);
        } else {
            card_layout->addWidget(explain);
        }

        std::vector<QWidget*> contents;
        if (row != unit && unit_box != nullptr) {
            for (int i = 0; i < unit_box->count(); ++i) {
                QWidget* const child = unit_box->itemAt(i)->widget();
                if (child != nullptr && child != explain) {
                    contents.push_back(child);
                }
            }
        }
        auto* const follower = new CardFollower(card, unit, row, explain, contents, guided);
        unit->installEventFilter(follower);
        if (row != unit) {
            row->installEventFilter(follower);
        }
        for (QWidget* const child : contents) {
            if (child != row) {
                child->installEventFilter(follower);
            }
        }
        QObject::connect(&CardsState::Instance(), &CardsState::CompactChanged, follower,
                         [follower](bool) { follower->Update(); });
        follower->Update();
    }
}

QWidget* AttachChoiceCards(QComboBox* combo, QWidget* row,
                           std::function<ChoiceText(int index)> describe, int columns) {
    if (combo == nullptr || row == nullptr || !describe) {
        return nullptr;
    }
    QWidget* const holder = row->parentWidget();
    auto* const box = holder != nullptr ? qobject_cast<QBoxLayout*>(holder->layout()) : nullptr;
    if (box == nullptr || box->indexOf(row) < 0) {
        return nullptr;
    }
    auto* const cards = new ChoiceCards(combo, row, std::move(describe), columns, holder);
    cards->setProperty("cervin_follows", true);
    box->insertWidget(box->indexOf(row) + 1, cards);
    combo->setVisible(false);
    combo->setProperty(ChoiceCardsProperty, QVariant::fromValue(static_cast<QObject*>(cards)));
    return cards;
}

void RefreshChoiceCards(QComboBox* combo) {
    if (combo == nullptr) {
        return;
    }
    QObject* const object = combo->property(ChoiceCardsProperty).value<QObject*>();
    if (auto* const cards = dynamic_cast<ChoiceCards*>(object)) {
        cards->ScheduleRebuild();
    }
}

SizeDiagram::SizeDiagram(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("CervinSizeDiagram"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void SizeDiagram::SetSizes(u32 base_height, u32 output_height) {
    base = base_height;
    output = output_height;
    update();
}

QSize SizeDiagram::sizeHint() const {
    return {176, 99};
}

void SizeDiagram::paintEvent(QPaintEvent*) {
    if (output == 0) {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect()).adjusted(1, 1, -1, -1);
    double out_width = area.width();
    double out_height = out_width * 9.0 / 16.0;
    if (out_height > area.height()) {
        out_height = area.height();
        out_width = out_height * 16.0 / 9.0;
    }
    const QRectF out(area.left(), area.top(), out_width, out_height);
    const double scale =
        std::clamp(static_cast<double>(base) / static_cast<double>(output), 0.05, 1.0);
    const QRectF in(out.left(), out.bottom() - out_height * scale, out_width * scale,
                    out_height * scale);

    painter.setPen(QPen(QColor(0x3a, 0x3a, 0x42), 1.0));
    painter.setBrush(QColor(0x18, 0x18, 0x1b));
    painter.drawRoundedRect(out, 6, 6);
    painter.setPen(QPen(QColor(0xa7, 0x8b, 0xfa), 1.5));
    painter.setBrush(QColor(0xa7, 0x8b, 0xfa, 56));
    painter.drawRoundedRect(in, 4, 4);

    QFont small = painter.font();
    small.setPointSizeF(small.pointSizeF() * 0.85);
    painter.setFont(small);
    painter.setPen(QColor(0x9a, 0x9a, 0xa6));
    painter.drawText(out.adjusted(8, 5, -8, -5), Qt::AlignTop | Qt::AlignRight,
                     CardsState::tr("output %1").arg(HeightText(output)));
    if (scale < 1.0) {
        painter.setPen(QColor(0xec, 0xec, 0xf1));
        painter.drawText(in.adjusted(6, 4, -6, -4), Qt::AlignBottom | Qt::AlignLeft,
                         CardsState::tr("base %1").arg(HeightText(base)));
    }
}

} // namespace ConfigurationShared
