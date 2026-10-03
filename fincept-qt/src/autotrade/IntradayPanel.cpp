#include "autotrade/IntradayPanel.h"

#include "autotrade/AutotradeApi.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QDateTime>
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

constexpr int kIntradayRefreshMs = 30000;

QTableWidget* intraday_table(QWidget* parent, const int columns) {
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

QLabel* intraday_section(QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName("intradaySection");
    return l;
}

QTableWidgetItem* it_txt(const QString& s) { return new QTableWidgetItem(s); }

QTableWidgetItem* it_num(const QJsonValue& v, int decimals, const QString& suffix = QString(), bool signed_color = false) {
    auto* it = new QTableWidgetItem(v.isDouble() ? QLocale().toString(v.toDouble(), 'f', decimals) + suffix
                                                 : QStringLiteral("—"));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (signed_color && v.isDouble() && v.toDouble() != 0)
        it->setForeground(QColor(v.toDouble() > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    return it;
}

QTableWidgetItem* it_pct(const QJsonValue& v, int decimals = 3) {
    if (!v.isDouble())
        return it_num(v, 0);
    return it_num(QJsonValue(v.toDouble() * 100), decimals, "%", true);
}

QString it_side(const QJsonValue& v) {
    if (!v.isDouble())
        return QStringLiteral("—");
    const int s = v.toInt();
    return s > 0 ? QStringLiteral("LONG") : (s < 0 ? QStringLiteral("SHORT") : QStringLiteral("FLAT"));
}

QString it_hhmm(const QString& iso) {
    const auto t = QDateTime::fromString(iso, Qt::ISODateWithMs);
    return t.isValid() ? t.toLocalTime().toString("HH:mm:ss") : iso;
}

} // namespace

IntradayPanel::IntradayPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    bar_ = new QWidget(this);
    auto* hb = new QHBoxLayout(bar_);
    hb->setContentsMargins(12, 6, 12, 6);
    auto* labels = new QVBoxLayout;
    labels->setSpacing(2);
    summary_lbl_ = new QLabel;
    summary_lbl_->setTextFormat(Qt::RichText);
    totals_lbl_ = new QLabel;
    totals_lbl_->setTextFormat(Qt::RichText);
    labels->addWidget(summary_lbl_);
    labels->addWidget(totals_lbl_);
    hb->addLayout(labels, 1);
    refresh_btn_ = new QPushButton;
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &IntradayPanel::load);
    hb->addWidget(refresh_btn_);
    root->addWidget(bar_);

    auto* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    auto box = [split](QLabel*& title, QTableWidget*& table, int cols) {
        auto* w = new QWidget(split);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);
        title = intraday_section(w);
        v->addWidget(title);
        table = intraday_table(w, cols);
        v->addWidget(table, 1);
        split->addWidget(w);
    };
    box(decisions_title_, decisions_, 11);
    box(fills_title_, fills_, 8);
    box(days_title_, days_, 9);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setStretchFactor(2, 2);
    root->addWidget(split, 1);

    note_lbl_ = new QLabel(this);
    note_lbl_->setWordWrap(true);
    note_lbl_->setContentsMargins(12, 6, 12, 8);
    root->addWidget(note_lbl_);

    timer_ = new QTimer(this);
    timer_->setInterval(kIntradayRefreshMs);
    connect(timer_, &QTimer::timeout, this, &IntradayPanel::load);
    retranslate();
}

void IntradayPanel::apply_styles() {
    const QString section = QString("color: %1; font-size: 10px; font-weight: 700; padding: 8px 12px; background: %2; "
                                    "border-top: 1px solid %3; border-bottom: 1px solid %3;")
                                .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM());
    for (QLabel* l : {decisions_title_, fills_title_, days_title_})
        if (l)
            l->setStyleSheet(section);
    if (bar_)
        bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    for (QLabel* l : {summary_lbl_, totals_lbl_})
        if (l)
            l->setStyleSheet(QString("color: %1; font-size: 11px; border: none;").arg(ui::colors::TEXT_SECONDARY()));
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
    for (QTableWidget* t : {decisions_, fills_, days_})
        if (t)
            t->setStyleSheet(css);
}

void IntradayPanel::retranslate() {
    refresh_btn_->setText(tr("REFRESH"));
    decisions_title_->setText(tr("DECISIONS (every 30 minutes, 10:00-15:30 ET; 15:59 = close)"));
    fills_title_->setText(tr("FILLS (IB paper)"));
    days_title_->setText(tr("DAILY RESULTS: REAL vs BACKTEST"));
    decisions_->setHorizontalHeaderLabels({tr("BAR"), tr("SIDE"), tr("TARGET"), tr("HELD"), tr("ORDER"), tr("CLOSE"),
                                           tr("UPPER BAND"), tr("LOWER BAND"), tr("VWAP"), tr("LEV"), tr("STATUS")});
    fills_->setHorizontalHeaderLabels({tr("TIME"), tr("FOR BAR"), tr("SIDE"), tr("SHARES"), tr("PRICE"),
                                       tr("DECISION PRICE"), tr("SLIPPAGE (bp)"), tr("COMMISSION")});
    days_->setHorizontalHeaderLabels({tr("DAY"), tr("TRADES"), tr("NET P&L"), tr("REAL RETURN"), tr("BACKTEST RETURN"),
                                      tr("DIFFERENCE"), tr("COMMISSIONS"), tr("CAPITAL END"), tr("MODE")});
    note_lbl_->setText(tr("Intraday momentum (noise area) on SPY: the strategy's own module and frozen config. History "
                          "from IBKR, live bars from Yahoo, paper orders on the IB paper account with a $100k book; "
                          "always flat at the close. BACKTEST RETURN is the strategy's backtest for the same day."));
    if (summary_lbl_->text().isEmpty())
        summary_lbl_->setText(tr("Loading…"));
}

void IntradayPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    load();
    timer_->start();
}

void IntradayPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    timer_->stop();
}

void IntradayPanel::load() {
    HttpClient::instance().get(
        api_url("/intraday/summary"),
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                summary_lbl_->setText(tr("Cannot reach %1").arg(api_base_url()).toHtmlEscaped());
                return;
            }
            render(r.value().object());
        },
        this);
}

void IntradayPanel::render(const QJsonObject& data) {
    const QString day = data.value("day").toString();
    const QString mode = data.value("mode").toString();
    const int position = data.value("position").toInt();
    const QString pos_text = position > 0   ? tr("LONG %1 SPY").arg(position)
                             : position < 0 ? tr("SHORT %1 SPY").arg(-position)
                                            : tr("FLAT");
    if (day.isEmpty()) {
        summary_lbl_->setText(tr("<b>INTRADAY MOMENTUM · SPY</b> · no decisions recorded yet "
                                 "(first decision at 10:00 ET on a trading day)"));
    } else {
        summary_lbl_->setText(tr("<b>INTRADAY MOMENTUM · SPY</b> · day <b>%1</b> · mode <b>%2</b> · position <b>%3</b> · "
                                 "capital <b>$%4</b>")
                                  .arg(day, mode.toUpper(), pos_text)
                                  .arg(QLocale().toString(data.value("capital").toDouble(), 'f', 0)));
    }
    const QJsonObject t = data.value("totals").toObject();
    if (t.value("live_days").toInt() > 0) {
        const double lr = t.value("live_return").toDouble(), br = t.value("backtest_return").toDouble();
        totals_lbl_->setText(tr("Since %1 (%2 days): real <b style='color:%3'>%4%</b> vs backtest <b>%5%</b> · "
                                "net P&L <b>$%6</b> · commissions $%7")
                                 .arg(t.value("first_day").toString())
                                 .arg(t.value("live_days").toInt())
                                 .arg(QString(lr >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()))
                                 .arg(lr * 100, 0, 'f', 2)
                                 .arg(br * 100, 0, 'f', 2)
                                 .arg(QLocale().toString(t.value("net_pnl").toDouble(), 'f', 2))
                                 .arg(QLocale().toString(t.value("commissions").toDouble(), 'f', 2)));
    } else {
        totals_lbl_->setText(tr("No completed trading days yet; the first day is recorded at 16:05 ET."));
    }

    const QJsonArray dec = data.value("decisions").toArray();
    decisions_->setRowCount(dec.size());
    for (int i = 0; i < dec.size(); ++i) {
        const QJsonObject d = dec.at(i).toObject();
        auto* side = it_txt(it_side(d.value("side")));
        if (d.value("side").isDouble() && d.value("side").toInt() != 0)
            side->setForeground(QColor(d.value("side").toInt() > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        decisions_->setItem(i, 0, it_txt(d.value("bar").toString()));
        decisions_->setItem(i, 1, side);
        decisions_->setItem(i, 2, it_num(d.value("target"), 0));
        decisions_->setItem(i, 3, it_num(d.value("held"), 0));
        decisions_->setItem(i, 4, it_num(d.value("order"), 0, QString(), true));
        decisions_->setItem(i, 5, it_num(d.value("close"), 2));
        decisions_->setItem(i, 6, it_num(d.value("ub"), 2));
        decisions_->setItem(i, 7, it_num(d.value("lb"), 2));
        decisions_->setItem(i, 8, it_num(d.value("vwap"), 2));
        decisions_->setItem(i, 9, it_num(d.value("leverage"), 2));
        const QString note = d.value("note").toString();
        decisions_->setItem(i, 10, it_txt(note.isEmpty() ? d.value("status").toString()
                                                      : d.value("status").toString() + " · " + note));
    }
    decisions_title_->setText(tr("DECISIONS %1 (every 30 minutes, 10:00-15:30 ET; 15:59 = close)").arg(day));

    const QJsonArray fl = data.value("fills").toArray();
    fills_->setRowCount(fl.size());
    for (int i = 0; i < fl.size(); ++i) {
        const QJsonObject f = fl.at(i).toObject();
        fills_->setItem(i, 0, it_txt(it_hhmm(f.value("at").toString())));
        fills_->setItem(i, 1, it_txt(f.value("bar").toString()));
        fills_->setItem(i, 2, it_txt(f.value("side").toString() == "BOT" ? tr("BUY") : tr("SELL")));
        fills_->setItem(i, 3, it_num(f.value("shares"), 0));
        fills_->setItem(i, 4, it_num(f.value("price"), 2));
        fills_->setItem(i, 5, it_num(f.value("decision_close"), 2));
        auto* slip = it_num(f.value("slippage_bp"), 2);
        if (f.value("slippage_bp").isDouble() && f.value("slippage_bp").toDouble() != 0)
            slip->setForeground(QColor(f.value("slippage_bp").toDouble() > 0 ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()));
        fills_->setItem(i, 6, slip);
        fills_->setItem(i, 7, it_num(f.value("commission"), 2));
    }
    fills_title_->setText(tr("FILLS %1 (IB paper) · %2").arg(day).arg(fl.size()));

    const QJsonArray ds = data.value("days").toArray();
    days_->setRowCount(ds.size());
    for (int i = 0; i < ds.size(); ++i) {
        const QJsonObject d = ds.at(i).toObject();
        const QJsonValue diff = (d.value("ret").isDouble() && d.value("backtest_ret").isDouble())
                                    ? QJsonValue(d.value("ret").toDouble() - d.value("backtest_ret").toDouble())
                                    : QJsonValue();
        days_->setItem(i, 0, it_txt(d.value("day").toString()));
        days_->setItem(i, 1, it_num(d.value("trades"), 0));
        days_->setItem(i, 2, it_num(d.value("net_pnl"), 2, QString(), true));
        days_->setItem(i, 3, it_pct(d.value("ret")));
        days_->setItem(i, 4, it_pct(d.value("backtest_ret")));
        days_->setItem(i, 5, it_pct(diff));
        days_->setItem(i, 6, it_num(d.value("commissions"), 2));
        days_->setItem(i, 7, it_num(d.value("capital_end"), 0));
        days_->setItem(i, 8, it_txt(d.value("dry_run").toBool() ? tr("dry run") : tr("paper")));
    }
}

} // namespace fincept::autotrade
