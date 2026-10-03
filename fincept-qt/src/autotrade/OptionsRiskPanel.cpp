#include "autotrade/OptionsRiskPanel.h"

#include "autotrade/AutotradeApi.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::autotrade {

namespace {

constexpr int kOptionsRiskRefreshMs = 60000;

QTableWidget* or_table(QWidget* parent, const int columns) {
    auto* t = new QTableWidget(parent);
    t->setColumnCount(columns);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setShowGrid(false);
    t->setAlternatingRowColors(true);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    return t;
}

QTableWidgetItem* or_txt(const QString& s) { return new QTableWidgetItem(s); }

QTableWidgetItem* or_num(const QJsonValue& v, int decimals, bool signed_color = false, double scale = 1.0) {
    auto* it = new QTableWidgetItem(v.isDouble() ? QLocale().toString(v.toDouble() * scale, 'f', decimals)
                                                 : QStringLiteral("—"));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (signed_color && v.isDouble() && v.toDouble() != 0)
        it->setForeground(QColor(v.toDouble() > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    return it;
}

QString or_money(double v) { return QLocale().toString(v, 'f', 0); }

} // namespace

OptionsRiskPanel::OptionsRiskPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    bar_ = new QWidget(this);
    auto* hb = new QHBoxLayout(bar_);
    hb->setContentsMargins(12, 6, 12, 6);
    summary_lbl_ = new QLabel;
    summary_lbl_->setTextFormat(Qt::RichText);
    summary_lbl_->setWordWrap(true);
    hb->addWidget(summary_lbl_, 1);
    refresh_btn_ = new QPushButton;
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &OptionsRiskPanel::load);
    hb->addWidget(refresh_btn_);
    root->addWidget(bar_);

    auto* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    auto box = [split](QLabel*& title, QTableWidget*& table, int cols) {
        auto* w = new QWidget(split);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);
        title = new QLabel(w);
        v->addWidget(title);
        table = or_table(w, cols);
        v->addWidget(table, 1);
        split->addWidget(w);
    };
    box(positions_title_, positions_, 15);
    box(stress_title_, stress_, 4);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    note_lbl_ = new QLabel(this);
    note_lbl_->setWordWrap(true);
    note_lbl_->setContentsMargins(12, 6, 12, 8);
    root->addWidget(note_lbl_);

    timer_ = new QTimer(this);
    timer_->setInterval(kOptionsRiskRefreshMs);
    connect(timer_, &QTimer::timeout, this, &OptionsRiskPanel::load);
    retranslate();
}

void OptionsRiskPanel::apply_styles() {
    const QString section = QString("color: %1; font-size: 10px; font-weight: 700; padding: 8px 12px; background: %2; "
                                    "border-top: 1px solid %3; border-bottom: 1px solid %3;")
                                .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM());
    for (QLabel* l : {positions_title_, stress_title_})
        if (l)
            l->setStyleSheet(section);
    if (bar_)
        bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    if (summary_lbl_)
        summary_lbl_->setStyleSheet(QString("color: %1; font-size: 11px; border: none;").arg(ui::colors::TEXT_SECONDARY()));
    if (note_lbl_)
        note_lbl_->setStyleSheet(QString("color: %1; font-size: 10px;").arg(ui::colors::TEXT_TERTIARY()));
    const QString css =
        QString("QTableWidget { background: %1; color: %2; border: none; gridline-color: %3; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()) +
        QString("QTableWidget::item { padding: 5px 8px; }") +
        QString("QTableWidget::item:alternate { background: %1; }").arg(ui::colors::BG_RAISED()) +
        QString("QHeaderView::section { background: %1; color: %2; border: none; border-bottom: 1px solid %3; "
                "padding: 6px 8px; font-size: 10px; font-weight: 700; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_MED());
    for (QTableWidget* t : {positions_, stress_})
        if (t)
            t->setStyleSheet(css);
}

void OptionsRiskPanel::retranslate() {
    refresh_btn_->setText(tr("REFRESH"));
    positions_title_->setText(tr("OPTION POSITIONS"));
    stress_title_->setText(tr("STRESS TEST"));
    positions_->setHorizontalHeaderLabels({tr("BROKER"), tr("SYMBOL"), tr("EXPIRY"), tr("DTE"), tr("RIGHT"),
                                           tr("STRIKE"), tr("QTY"), tr("MARK"), tr("IV %"), tr("DELTA (sh)"),
                                           tr("GAMMA"), tr("THETA $/day"), tr("VEGA $/pt"), tr("MARKET VALUE"),
                                           tr("STRATEGY")});
    stress_->setHorizontalHeaderLabels({tr("SCENARIO"), tr("P&L"), tr("VALUE AFTER"), tr("DELTA AFTER")});
    note_lbl_->setText(tr("Read-only. Option positions from IBKR, Tastytrade and Alpaca, priced off the market data "
                          "hub's latest chain (Tastytrade first). Delta is in shares, theta in $ per day, vega in $ "
                          "per vol point, for the whole position; shares in the same underlyings are included as "
                          "hedges. Stress is a full Black-Scholes revaluation of the book under each scenario."));
    if (summary_lbl_->text().isEmpty())
        summary_lbl_->setText(tr("Loading…"));
}

void OptionsRiskPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    load();
    timer_->start();
}

void OptionsRiskPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    timer_->stop();
}

void OptionsRiskPanel::load() {
    HttpClient::instance().get(
        api_url("/platform/options-book"),
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                summary_lbl_->setText(tr("Cannot reach %1").arg(api_base_url()).toHtmlEscaped());
                return;
            }
            render(r.value().object());
        },
        this);
}

void OptionsRiskPanel::render(const QJsonObject& data) {
    const QJsonArray rows = data.value("positions").toArray();
    const QJsonObject totals = data.value("totals").toObject();
    const QJsonObject risk = data.value("risk").toObject();
    int options = 0;
    for (const auto& v : rows)
        options += v.toObject().value("right").toString() != "SHARES";

    if (options == 0) {
        summary_lbl_->setText(tr("<b>OPTIONS RISK</b> · no option positions in any account"));
    } else {
        QString worst;
        if (risk.contains("worst_stress_loss"))
            worst = tr(" · worst stress loss <b style='color:%1'>$%2</b>")
                        .arg(QString(ui::colors::NEGATIVE()))
                        .arg(or_money(risk.value("worst_stress_loss").toDouble()));
        else if (risk.contains("error"))
            worst = tr(" · stress unavailable (%1)").arg(risk.value("error").toString().toHtmlEscaped());
        summary_lbl_->setText(tr("<b>OPTIONS RISK</b> · %1 option positions · delta <b>%2</b> sh · gamma <b>%3</b> · "
                                 "theta <b>$%4</b>/day · vega <b>$%5</b>/pt%6")
                                  .arg(options)
                                  .arg(QLocale().toString(totals.value("delta").toDouble(), 'f', 0))
                                  .arg(QLocale().toString(totals.value("gamma").toDouble(), 'f', 2))
                                  .arg(or_money(totals.value("theta").toDouble()))
                                  .arg(or_money(totals.value("vega").toDouble()))
                                  .arg(worst));
    }

    positions_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject p = rows.at(i).toObject();
        const bool shares = p.value("right").toString() == "SHARES";
        positions_->setItem(i, 0, or_txt(p.value("broker").toString().toUpper()));
        positions_->setItem(i, 1, or_txt(p.value("symbol").toString()));
        positions_->setItem(i, 2, or_txt(shares ? QString() : p.value("expiration").toString()));
        positions_->setItem(i, 3, shares ? or_txt(QString()) : or_num(p.value("dte"), 0));
        positions_->setItem(i, 4, or_txt(p.value("right").toString()));
        positions_->setItem(i, 5, shares ? or_txt(QString()) : or_num(p.value("strike"), 2));
        positions_->setItem(i, 6, or_num(p.value("quantity"), 0, true));
        positions_->setItem(i, 7, or_num(p.value("mark"), 2));
        positions_->setItem(i, 8, or_num(p.value("iv"), 1, false, 100.0));
        positions_->setItem(i, 9, or_num(p.value("delta"), 0, true));
        positions_->setItem(i, 10, or_num(p.value("gamma"), 2, true));
        positions_->setItem(i, 11, or_num(p.value("theta"), 0, true));
        positions_->setItem(i, 12, or_num(p.value("vega"), 0, true));
        positions_->setItem(i, 13, or_num(p.value("market_value"), 0, true));
        QStringList owners;
        for (const auto& s : p.value("strategies").toArray())
            owners << s.toString();
        positions_->setItem(i, 14, or_txt(owners.isEmpty() ? QStringLiteral("—") : owners.join(", ")));
    }
    positions_title_->setText(tr("OPTION POSITIONS · %1").arg(options));

    const QJsonArray stress = risk.value("stress").toArray();
    stress_->setRowCount(stress.size());
    for (int i = 0; i < stress.size(); ++i) {
        const QJsonObject s = stress.at(i).toObject();
        stress_->setItem(i, 0, or_txt(s.value("scenario").toString()));
        stress_->setItem(i, 1, or_num(s.value("pnl"), 0, true));
        stress_->setItem(i, 2, or_num(s.value("mv_after"), 0));
        stress_->setItem(i, 3, or_num(s.value("delta_after"), 0, true));
    }

    QStringList notes;
    for (const auto& n : data.value("notes").toArray())
        notes << n.toString();
    const QJsonObject chains = data.value("chains").toObject();
    for (auto it = chains.begin(); it != chains.end(); ++it)
        notes << tr("%1 chain %2").arg(it.key(), it.value().isString()
                                                     ? it.value().toString().left(19).replace('T', ' ') + " UTC"
                                                     : tr("missing"));
    stress_title_->setText(notes.isEmpty() ? tr("STRESS TEST") : tr("STRESS TEST · %1").arg(notes.join(" · ")));
}

} // namespace fincept::autotrade
