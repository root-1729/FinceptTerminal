#include "autotrade/PlatformStrategiesPanel.h"

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
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::autotrade {

namespace {

constexpr int kPlatformRefreshMs = 30000;

QTableWidget* ps_table(QWidget* parent, const int columns) {
    auto* t = new QTableWidget(parent);
    t->setColumnCount(columns);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setShowGrid(false);
    t->setAlternatingRowColors(true);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    return t;
}

QTableWidgetItem* ps_txt(const QString& s) { return new QTableWidgetItem(s); }

QTableWidgetItem* ps_num(const QJsonValue& v, int decimals, const QString& suffix = QString(), bool signed_color = false) {
    auto* it = new QTableWidgetItem(v.isDouble() ? QLocale().toString(v.toDouble(), 'f', decimals) + suffix
                                                 : QStringLiteral("—"));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (signed_color && v.isDouble() && v.toDouble() != 0)
        it->setForeground(QColor(v.toDouble() > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    return it;
}

QTableWidgetItem* ps_pct(const QJsonValue& v, int decimals = 2, bool signed_color = true) {
    if (!v.isDouble())
        return ps_num(v, 0);
    return ps_num(QJsonValue(v.toDouble() * 100), decimals, "%", signed_color);
}

/// {"SPY": 400} -> "SPY 400"; {"TQQQ": 0.8, "SQQQ": 0} -> "TQQQ 0.8 · SQQQ 0"
QString ps_map(const QJsonValue& v) {
    if (!v.isObject())
        return QStringLiteral("—");
    QStringList parts;
    const QJsonObject o = v.toObject();
    for (auto it = o.begin(); it != o.end(); ++it)
        parts << it.key() + " " + QLocale().toString(it.value().toDouble(), 'g', 6);
    return parts.join(" · ");
}

QString ps_time(const QString& iso) {
    const auto t = QDateTime::fromString(iso, Qt::ISODateWithMs);
    return t.isValid() ? t.toLocalTime().toString("yyyy-MM-dd HH:mm:ss") : iso;
}

QColor ps_stage_color(const QString& stage) {
    if (stage == "live")
        return QColor(ui::colors::NEGATIVE());   // real money: stands out
    if (stage == "paper")
        return QColor(ui::colors::POSITIVE());
    if (stage == "shadow")
        return QColor(ui::colors::AMBER());
    return QColor(ui::colors::TEXT_TERTIARY());
}

} // namespace

PlatformStrategiesPanel::PlatformStrategiesPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    bar_ = new QWidget(this);
    auto* hb = new QHBoxLayout(bar_);
    hb->setContentsMargins(12, 6, 12, 6);
    summary_lbl_ = new QLabel;
    summary_lbl_->setTextFormat(Qt::RichText);
    hb->addWidget(summary_lbl_, 1);
    refresh_btn_ = new QPushButton;
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &PlatformStrategiesPanel::load);
    hb->addWidget(refresh_btn_);
    root->addWidget(bar_);

    auto* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    list_ = ps_table(split, 12);
    connect(list_, &QTableWidget::itemSelectionChanged, this, [this] {
        const auto rows = list_->selectionModel()->selectedRows();
        if (rows.isEmpty())
            return;
        const QString id = list_->item(rows.first().row(), 0)->text();
        if (id != selected_) {
            selected_ = id;
            load_detail();
        }
    });
    split->addWidget(list_);

    auto* detail = new QWidget(split);
    auto* dv = new QVBoxLayout(detail);
    dv->setContentsMargins(0, 0, 0, 0);
    dv->setSpacing(0);
    detail_lbl_ = new QLabel(detail);
    detail_lbl_->setObjectName("platformDetail");
    detail_lbl_->setTextFormat(Qt::RichText);
    detail_lbl_->setWordWrap(true);
    dv->addWidget(detail_lbl_);
    detail_tabs_ = new QTabWidget(detail);
    detail_tabs_->setDocumentMode(true);
    daily_ = ps_table(detail_tabs_, 9);
    decisions_ = ps_table(detail_tabs_, 8);
    fills_ = ps_table(detail_tabs_, 9);
    history_ = ps_table(detail_tabs_, 5);
    detail_tabs_->addTab(daily_, QString());
    detail_tabs_->addTab(decisions_, QString());
    detail_tabs_->addTab(fills_, QString());
    detail_tabs_->addTab(history_, QString());
    dv->addWidget(detail_tabs_, 1);
    split->addWidget(detail);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    root->addWidget(split, 1);

    note_lbl_ = new QLabel(this);
    note_lbl_->setWordWrap(true);
    note_lbl_->setContentsMargins(12, 6, 12, 8);
    root->addWidget(note_lbl_);

    timer_ = new QTimer(this);
    timer_->setInterval(kPlatformRefreshMs);
    connect(timer_, &QTimer::timeout, this, &PlatformStrategiesPanel::load);
    retranslate();
}

void PlatformStrategiesPanel::apply_styles() {
    if (bar_)
        bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    if (summary_lbl_)
        summary_lbl_->setStyleSheet(QString("color: %1; font-size: 11px; border: none;").arg(ui::colors::TEXT_SECONDARY()));
    if (detail_lbl_)
        detail_lbl_->setStyleSheet(QString("color: %1; font-size: 11px; padding: 8px 12px; background: %2; "
                                           "border-top: 1px solid %3; border-bottom: 1px solid %3;")
                                       .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    if (note_lbl_)
        note_lbl_->setStyleSheet(QString("color: %1; font-size: 10px;").arg(ui::colors::TEXT_TERTIARY()));
    const QString css =
        QString("QTableWidget { background: %1; color: %2; border: none; gridline-color: %3; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()) +
        QString("QTableWidget::item { padding: 5px 8px; }") +
        QString("QTableWidget::item:alternate { background: %1; }").arg(ui::colors::BG_RAISED()) +
        QString("QTableWidget::item:selected { background: %1; color: %2; }").arg(ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY()) +
        QString("QHeaderView::section { background: %1; color: %2; border: none; border-bottom: 1px solid %3; "
                "padding: 6px 8px; font-size: 10px; font-weight: 700; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_MED());
    for (QTableWidget* t : {list_, daily_, decisions_, fills_, history_})
        if (t)
            t->setStyleSheet(css);
    if (detail_tabs_)
        detail_tabs_->setStyleSheet(
            QString("QTabBar::tab { background: %1; color: %2; padding: 6px 14px; font-size: 10px; font-weight: 700; "
                    "border: none; } QTabBar::tab:selected { color: %3; border-bottom: 2px solid %4; }")
                .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER()));
}

void PlatformStrategiesPanel::retranslate() {
    refresh_btn_->setText(tr("REFRESH"));
    list_->setHorizontalHeaderLabels({tr("ID"), tr("NAME"), tr("STAGE"), tr("ORDERS"), tr("ACCOUNT"), tr("CAPITAL"),
                                      tr("LIVE"), tr("BACKTEST"), tr("DRAWDOWN"), tr("DAYS"), tr("LAST DECISION"),
                                      tr("SHADOW")});
    detail_tabs_->setTabText(0, tr("DAILY: LIVE vs BACKTEST"));
    detail_tabs_->setTabText(1, tr("DECISIONS"));
    detail_tabs_->setTabText(2, tr("FILLS"));
    detail_tabs_->setTabText(3, tr("STAGE HISTORY"));
    daily_->setHorizontalHeaderLabels({tr("DAY"), tr("RETURN"), tr("BACKTEST"), tr("DIFFERENCE"), tr("NAV"),
                                       tr("NET P&L"), tr("TRADES"), tr("COMMISSIONS"), tr("MODE")});
    decisions_->setHorizontalHeaderLabels({tr("BAR"), tr("DECISION"), tr("TARGET"), tr("HELD"), tr("ORDER"),
                                           tr("STATUS"), tr("TIME"), tr("NOTE")});
    fills_->setHorizontalHeaderLabels({tr("TIME"), tr("FOR"), tr("SYMBOL"), tr("SIDE"), tr("QTY"), tr("PRICE"),
                                       tr("DECISION PRICE"), tr("SLIPPAGE (bp)"), tr("COMMISSION")});
    history_->setHorizontalHeaderLabels({tr("DATE"), tr("FROM"), tr("TO"), tr("BY"), tr("EVIDENCE")});
    note_lbl_->setText(tr("Strategies from the autotrade registry (strategies/<id>/strategy.yaml). Stages: idea → "
                          "registered → backtest-passed → shadow (decisions, no orders) → paper → live; only paper and "
                          "live may send orders, each step needs recorded evidence. LIVE compounds the strategy's daily "
                          "results (paper NAV for a dry run); BACKTEST is the strategy's backtest for the same days."));
    if (summary_lbl_->text().isEmpty())
        summary_lbl_->setText(tr("Loading…"));
    if (detail_lbl_->text().isEmpty())
        detail_lbl_->setText(tr("Select a strategy for its history, decisions and fills."));
}

void PlatformStrategiesPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    load();
    timer_->start();
}

void PlatformStrategiesPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    timer_->stop();
}

void PlatformStrategiesPanel::load() {
    HttpClient::instance().get(
        api_url("/platform/strategies"),
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                summary_lbl_->setText(tr("Cannot reach %1").arg(api_base_url()).toHtmlEscaped());
                return;
            }
            render_list(r.value().object());
            if (!selected_.isEmpty())
                load_detail();
        },
        this);
}

void PlatformStrategiesPanel::load_detail() {
    if (selected_.isEmpty())
        return;
    const QString id = selected_;
    HttpClient::instance().get(
        api_url("/platform/strategies/" + QString::fromLatin1(QUrl::toPercentEncoding(id))),
        [this, id](Result<QJsonDocument> r) {
            if (id != selected_)
                return;  // selection moved on
            if (r.is_err()) {
                detail_lbl_->setText(tr("Cannot load %1").arg(id).toHtmlEscaped());
                return;
            }
            render_detail(r.value().object());
        },
        this);
}

void PlatformStrategiesPanel::render_list(const QJsonObject& data) {
    const QJsonArray rows = data.value("strategies").toArray();
    int ordering = 0;
    for (const auto& v : rows)
        ordering += v.toObject().value("orders").toString() == "allowed";
    summary_lbl_->setText(tr("<b>STRATEGIES</b> · %1 registered · %2 may send orders").arg(rows.size()).arg(ordering));

    const QSignalBlocker block(list_);
    list_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject s = rows.at(i).toObject();
        const QString stage = s.value("stage").toString();
        auto* st = ps_txt(stage.toUpper());
        st->setForeground(ps_stage_color(stage));
        auto* orders = ps_txt(s.value("orders").toString() == "allowed" ? tr("ALLOWED") : tr("off"));
        if (s.value("orders").toString() == "allowed")
            orders->setForeground(QColor(ui::colors::POSITIVE()));
        const QJsonObject last = s.value("last_decision").toObject();
        const QString last_txt = last.isEmpty() ? QStringLiteral("—")
                                                : QString("%1 %2 · %3 · %4")
                                                      .arg(last.value("day").toString(), last.value("label").toString(),
                                                           ps_map(last.value("targets")), last.value("status").toString());
        QString shadow = QStringLiteral("—");
        if (s.contains("shadow_of"))
            shadow = tr("vs %1: %2/%3 days match")
                         .arg(s.value("shadow_of").toString())
                         .arg(s.value("shadow_matching_days").toInt())
                         .arg(s.value("shadow_days").toInt());
        list_->setItem(i, 0, ps_txt(s.value("id").toString()));
        list_->setItem(i, 1, ps_txt(s.value("name").toString()));
        list_->setItem(i, 2, st);
        list_->setItem(i, 3, orders);
        list_->setItem(i, 4, ps_txt(s.value("broker").toString().toUpper() + " · " + s.value("account").toString()));
        list_->setItem(i, 5, ps_num(s.value("capital_usd"), 0));
        list_->setItem(i, 6, ps_pct(s.value("live_return")));
        list_->setItem(i, 7, ps_pct(s.value("backtest_return")));
        list_->setItem(i, 8, ps_pct(s.value("drawdown").isDouble() ? QJsonValue(-s.value("drawdown").toDouble())
                                                                   : QJsonValue()));
        list_->setItem(i, 9, ps_num(s.value("days"), 0));
        list_->setItem(i, 10, ps_txt(last_txt));
        list_->setItem(i, 11, ps_txt(shadow));
        if (s.value("id").toString() == selected_)
            list_->selectRow(i);
    }
    if (selected_.isEmpty() && !rows.isEmpty()) {
        selected_ = rows.first().toObject().value("id").toString();
        list_->selectRow(0);
        load_detail();
    }
}

void PlatformStrategiesPanel::render_detail(const QJsonObject& d) {
    const QJsonObject t = d.value("track").toObject();
    QString head = tr("<b>%1</b> · %2 · stage <b>%3</b> · orders <b>%4</b> · %5 · config <code>%6</code>")
                       .arg(d.value("id").toString().toHtmlEscaped(), d.value("name").toString().toHtmlEscaped(),
                            d.value("stage").toString().toUpper(), d.value("orders").toString(),
                            d.value("code").toString().toHtmlEscaped(), d.value("config_hash").toString());
    if (t.value("days").toInt() > 0)
        head += tr("<br>Since %1 (%2 days): live <b>%3%</b> · backtest <b>%4</b> · max drawdown %5% · net P&L $%6")
                    .arg(t.value("first_day").toString())
                    .arg(t.value("days").toInt())
                    .arg(t.value("live_return").toDouble() * 100, 0, 'f', 2)
                    .arg(t.value("backtest_return").isDouble()
                             ? QString::number(t.value("backtest_return").toDouble() * 100, 'f', 2) + "%"
                             : QStringLiteral("—"))
                    .arg(t.value("max_drawdown").toDouble() * 100, 0, 'f', 2)
                    .arg(QLocale().toString(t.value("net_pnl").toDouble(), 'f', 2));
    const QJsonObject shadow = d.value("shadow").toObject();
    if (!d.value("shadow_of").toString().isEmpty()) {
        int bars = 0, diff = 0;
        for (auto it = shadow.begin(); it != shadow.end(); ++it) {
            bars += it.value().toObject().value("bars").toInt();
            diff += it.value().toObject().value("mismatches").toArray().size();
        }
        head += tr("<br>Shadow of <b>%1</b>: %2 days, %3 decision bars, %4 with a different target")
                    .arg(d.value("shadow_of").toString())
                    .arg(shadow.size())
                    .arg(bars)
                    .arg(diff);
    }
    detail_lbl_->setText(head);

    const QJsonArray daily = d.value("daily").toArray();
    daily_->setRowCount(daily.size());
    for (int i = 0; i < daily.size(); ++i) {
        const QJsonObject r = daily.at(daily.size() - 1 - i).toObject();  // newest first
        const QJsonValue diffv = (r.value("ret").isDouble() && r.value("backtest_ret").isDouble())
                                     ? QJsonValue(r.value("ret").toDouble() - r.value("backtest_ret").toDouble())
                                     : QJsonValue();
        daily_->setItem(i, 0, ps_txt(r.value("day").toString()));
        daily_->setItem(i, 1, ps_pct(r.value("ret"), 3));
        daily_->setItem(i, 2, ps_pct(r.value("backtest_ret"), 3));
        daily_->setItem(i, 3, ps_pct(diffv, 3));
        daily_->setItem(i, 4, ps_num(r.value("nav"), 4));
        daily_->setItem(i, 5, ps_num(r.value("net_pnl"), 2, QString(), true));
        daily_->setItem(i, 6, ps_num(r.value("trades"), 0));
        daily_->setItem(i, 7, ps_num(r.value("commissions"), 2));
        daily_->setItem(i, 8, ps_txt(r.value("dry_run").toBool() ? tr("dry run") : tr("orders")));
    }

    const QJsonArray dec = d.value("decisions").toArray();
    decisions_->setRowCount(dec.size());
    for (int i = 0; i < dec.size(); ++i) {
        const QJsonObject r = dec.at(dec.size() - 1 - i).toObject();  // API is newest first; show the day in order
        decisions_->setItem(i, 0, ps_txt(r.value("label").toString()));
        decisions_->setItem(i, 1, ps_txt(r.value("decision").toString()));
        decisions_->setItem(i, 2, ps_txt(ps_map(r.value("targets"))));
        decisions_->setItem(i, 3, ps_txt(ps_map(r.value("held"))));
        decisions_->setItem(i, 4, ps_txt(ps_map(r.value("orders"))));
        decisions_->setItem(i, 5, ps_txt(r.value("status").toString()));
        decisions_->setItem(i, 6, ps_txt(ps_time(r.value("decided_at").toString())));
        decisions_->setItem(i, 7, ps_txt(r.value("note").toString()));
    }
    detail_tabs_->setTabText(1, tr("DECISIONS %1").arg(d.value("decisions_day").toString()));

    const QJsonArray fl = d.value("fills").toArray();
    fills_->setRowCount(fl.size());
    for (int i = 0; i < fl.size(); ++i) {
        const QJsonObject f = fl.at(i).toObject();
        fills_->setItem(i, 0, ps_txt(ps_time(f.value("filled_at").toString())));
        fills_->setItem(i, 1, ps_txt(f.value("label").toString()));
        fills_->setItem(i, 2, ps_txt(f.value("symbol").toString()));
        fills_->setItem(i, 3, ps_txt(f.value("side").toString()));
        fills_->setItem(i, 4, ps_num(f.value("quantity"), 0));
        fills_->setItem(i, 5, ps_num(f.value("price"), 2));
        fills_->setItem(i, 6, ps_num(f.value("decision_price"), 2));
        auto* slip = ps_num(f.value("slippage_bp"), 2);
        if (f.value("slippage_bp").isDouble() && f.value("slippage_bp").toDouble() != 0)
            slip->setForeground(QColor(f.value("slippage_bp").toDouble() > 0 ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()));
        fills_->setItem(i, 7, slip);
        fills_->setItem(i, 8, ps_num(f.value("commission"), 2));
    }
    detail_tabs_->setTabText(2, tr("FILLS · %1").arg(fl.size()));

    const QJsonArray hist = d.value("history").toArray();
    history_->setRowCount(hist.size());
    for (int i = 0; i < hist.size(); ++i) {
        const QJsonObject h = hist.at(hist.size() - 1 - i).toObject();  // newest first
        QStringList ev;
        const QJsonObject e = h.value("evidence").toObject();
        for (auto it = e.begin(); it != e.end(); ++it)
            ev << it.key() + "=" + it.value().toVariant().toString();
        history_->setItem(i, 0, ps_txt(h.value("at").toString()));
        history_->setItem(i, 1, ps_txt(h.value("from").toString()));
        auto* to = ps_txt(h.value("to").toString().toUpper());
        to->setForeground(ps_stage_color(h.value("to").toString()));
        history_->setItem(i, 2, to);
        history_->setItem(i, 3, ps_txt(h.value("by").toString()));
        history_->setItem(i, 4, ps_txt(ev.join("; ")));
    }
}

} // namespace fincept::autotrade
