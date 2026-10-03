#include "autotrade/ConditionsPanel.h"

#include "autotrade/AutotradeApi.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::autotrade {

namespace {

enum CondCol { CCond, CToday, CThird, CRange, CAvg, CWin, CQqq, CRank, CColCount };

QString fmt_value(double v, const QString& unit) {
    if (unit == "pct")
        return QString("%1%2%").arg(v > 0 ? "+" : "").arg(v * 100, 0, 'f', 1);
    if (unit == "pp")
        return QString("%1%2 pp").arg(v > 0 ? "+" : "").arg(v, 0, 'f', 2);
    if (unit == "pct_points")
        return QString("%1%").arg(v, 0, 'f', 2);
    if (unit == "ratio")
        return QString::number(v, 'f', 3);
    return QString::number(v, 'f', 1);
}

QTableWidgetItem* num_item(const QString& text, QColor color = QColor()) {
    auto* it = new QTableWidgetItem(text);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (color.isValid())
        it->setForeground(color);
    return it;
}

} // namespace

ConditionsPanel::ConditionsPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    bar_ = new QWidget(this);
    auto* hb = new QHBoxLayout(bar_);
    hb->setContentsMargins(12, 6, 12, 6);
    hb->setSpacing(8);
    model_lbl_ = new QLabel;
    hb->addWidget(model_lbl_);
    model_combo_ = new QComboBox;
    model_combo_->setMinimumWidth(220);
    connect(model_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { load(); });
    hb->addWidget(model_combo_);
    summary_lbl_ = new QLabel;
    summary_lbl_->setTextFormat(Qt::RichText);
    hb->addWidget(summary_lbl_, 1);
    refresh_btn_ = new QPushButton;
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &ConditionsPanel::load);
    hb->addWidget(refresh_btn_);
    root->addWidget(bar_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(CColCount);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(CCond, QHeaderView::Stretch);
    root->addWidget(table_, 1);

    note_lbl_ = new QLabel(this);
    note_lbl_->setWordWrap(true);
    note_lbl_->setContentsMargins(12, 6, 12, 8);
    root->addWidget(note_lbl_);

    retranslate();
}

void ConditionsPanel::apply_styles() {
    if (bar_)
        bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    if (model_lbl_)
        model_lbl_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; border: none;")
                                      .arg(ui::colors::TEXT_TERTIARY()));
    if (summary_lbl_)
        summary_lbl_->setStyleSheet(QString("color: %1; font-size: 11px; border: none;").arg(ui::colors::TEXT_SECONDARY()));
    if (note_lbl_)
        note_lbl_->setStyleSheet(QString("color: %1; font-size: 10px;").arg(ui::colors::TEXT_TERTIARY()));
    if (table_)
        table_->setStyleSheet(
            QString("QTableWidget { background: %1; color: %2; border: none; gridline-color: %3; }")
                .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()) +
            QString("QTableWidget::item { padding: 6px 8px; }") +
            QString("QTableWidget::item:alternate { background: %1; }").arg(ui::colors::BG_RAISED()) +
            QString("QHeaderView::section { background: %1; color: %2; border: none; border-bottom: 1px solid %3; "
                    "padding: 6px 8px; font-size: 10px; font-weight: 700; }")
                .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_MED()));
}

void ConditionsPanel::retranslate() {
    model_lbl_->setText(tr("STRATEGY"));
    refresh_btn_->setText(tr("REFRESH"));
    table_->setHorizontalHeaderLabels({tr("CONDITION"), tr("TODAY"), tr("THIRD"), tr("THIRD'S RANGE"),
                                       tr("STRATEGY AVG/MONTH"), tr("WIN %"), tr("QQQ AVG/MONTH"),
                                       tr("FOR THIS STRATEGY")});
    note_lbl_->setText(tr("Each condition's value today, placed in the low / mid / high third of its history since "
                          "2011, with how the strategy did in months that started in that third. Descriptive only: "
                          "conditions overlap, and in walk-forward tests (/research/filter_test) cutting exposure in "
                          "a strategy's 'worst' third did not improve out-of-sample results."));
    if (summary_lbl_->text().isEmpty())
        summary_lbl_->setText(tr("Loading conditions…"));
}

void ConditionsPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!models_loaded_)
        load_models();
    else
        load();
}

void ConditionsPanel::load_models() {
    HttpClient::instance().get(
        api_url("/rotation/strategies"),
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                summary_lbl_->setText(tr("Cannot reach %1").arg(api_base_url()).toHtmlEscaped());
                return;
            }
            QSignalBlocker block(model_combo_);
            model_combo_->clear();
            int active = 0;
            const QJsonArray rows = r.value().object().value("strategies").toArray();
            for (int i = 0; i < rows.size(); ++i) {
                const QJsonObject s = rows.at(i).toObject();
                model_combo_->addItem(s.value("model").toString());
                if (s.value("active").toBool())
                    active = i;
            }
            model_combo_->setCurrentIndex(active);
            models_loaded_ = true;
            load();
        },
        this);
}

void ConditionsPanel::load() {
    const QString model = model_combo_->currentText();
    if (model.isEmpty())
        return;
    summary_lbl_->setText(tr("Loading %1…").arg(model.toHtmlEscaped()));
    HttpClient::instance().get(
        api_url("/research/conditions?model=") + QUrl::toPercentEncoding(model),
        [this, model](Result<QJsonDocument> r) {
            if (model != model_combo_->currentText())
                return;
            if (r.is_err()) {
                summary_lbl_->setText(tr("Conditions unavailable: %1")
                                          .arg(HttpClient::message_from_error(r.error()))
                                          .toHtmlEscaped());
                return;
            }
            render(r.value().object());
        },
        this);
}

void ConditionsPanel::render(const QJsonObject& data) {
    const QJsonObject overall = data.value("overall").toObject();
    const QJsonArray rows = data.value("rows").toArray();
    int good = 0, bad = 0;
    for (const QJsonValue& v : rows) {
        const QString rank = v.toObject().value("rank").toString();
        good += rank == "best";
        bad += rank == "worst";
    }
    summary_lbl_->setText(tr("As of <b>%1</b> · strategy averages <b>%2%</b>/month overall (QQQ %3%) · "
                             "today: <b style='color:%4'>%5 favourable</b>, <b style='color:%6'>%7 unfavourable</b> of %8")
                              .arg(data.value("as_of").toString())
                              .arg(overall.value("avg").toDouble() * 100, 0, 'f', 2)
                              .arg(overall.value("qqq_avg").toDouble() * 100, 0, 'f', 2)
                              .arg(QString(ui::colors::POSITIVE())).arg(good)
                              .arg(QString(ui::colors::NEGATIVE())).arg(bad)
                              .arg(rows.size()));

    const double base = overall.value("avg").toDouble();
    table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject c = rows.at(i).toObject();
        const QString unit = c.value("unit").toString();
        const QString third = c.value("bucket").toString();
        const QString range = third == "low"    ? tr("≤ %1").arg(fmt_value(c.value("low_cut").toDouble(), unit))
                              : third == "high" ? tr("> %1").arg(fmt_value(c.value("high_cut").toDouble(), unit))
                                                : tr("%1 to %2")
                                                      .arg(fmt_value(c.value("low_cut").toDouble(), unit),
                                                           fmt_value(c.value("high_cut").toDouble(), unit));
        const double avg = c.value("avg").toDouble();
        const QString rank = c.value("rank").toString();
        const QColor rank_color = rank == "best"    ? QColor(ui::colors::POSITIVE())
                                  : rank == "worst" ? QColor(ui::colors::NEGATIVE())
                                                    : QColor(ui::colors::TEXT_SECONDARY());
        table_->setItem(i, CCond, new QTableWidgetItem(c.value("condition").toString()));
        table_->setItem(i, CToday, num_item(fmt_value(c.value("value").toDouble(), unit)));
        table_->setItem(i, CThird, new QTableWidgetItem(third.toUpper()));
        table_->setItem(i, CRange, new QTableWidgetItem(range));
        table_->setItem(i, CAvg, num_item(QString("%1%2%").arg(avg > 0 ? "+" : "").arg(avg * 100, 0, 'f', 1),
                                         QColor(avg >= base ? ui::colors::POSITIVE() : ui::colors::NEGATIVE())));
        table_->setItem(i, CWin, num_item(QString("%1%").arg(c.value("win").toDouble() * 100, 0, 'f', 0)));
        const double q = c.value("qqq_avg").toDouble();
        table_->setItem(i, CQqq, num_item(QString("%1%2%").arg(q > 0 ? "+" : "").arg(q * 100, 0, 'f', 1)));
        auto* rank_item = new QTableWidgetItem(rank == "best"    ? tr("best third")
                                               : rank == "worst" ? tr("worst third")
                                                                 : tr("middle"));
        rank_item->setForeground(rank_color);
        table_->setItem(i, CRank, rank_item);
    }
}

} // namespace fincept::autotrade
