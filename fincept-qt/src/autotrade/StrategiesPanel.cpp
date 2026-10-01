#include "autotrade/StrategiesPanel.h"

#include "autotrade/AutotradeApi.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLocale>
#include <QSignalBlocker>
#include <QPushButton>
#include <QSplitter>
#include <QUrl>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::autotrade {

namespace {

constexpr int kStrategiesRefreshMs = 60000;

constexpr double kHistoryCapital = 100000.0;

enum Col {
    Model, Decision, Tqqq, Sqqq, LiveRet, DayRet, LiveDd, Days,
    BtCagr, BtDd, BtSharpe, Bt12m, ColCount
};

enum HistCol { HDate, HDecision, HTqqq, HSqqq, HTrade, HHolding, HValue, HDay, HTotal, HColCount };

/// Item that sorts by a numeric value but shows formatted text.
class NumItem : public QTableWidgetItem {
  public:
    NumItem(const QString& text, double value) : QTableWidgetItem(text) {
        setData(Qt::UserRole, value);
        setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    bool operator<(const QTableWidgetItem& other) const override {
        return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble();
    }
};

QTableWidgetItem* pct(const QJsonValue& v, bool signed_color, int decimals = 1) {
    if (!v.isDouble()) {
        auto* item = new NumItem(QStringLiteral("—"), -1e9);
        item->setForeground(QColor(ui::colors::TEXT_DIM()));
        return item;
    }
    const double d = v.toDouble();
    auto* item = new NumItem(QString("%1%2%").arg(d > 0 && signed_color ? "+" : "").arg(d * 100, 0, 'f', decimals), d);
    if (signed_color && d != 0)
        item->setForeground(QColor(d > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    return item;
}

QTableWidgetItem* num(const QJsonValue& v, int decimals) {
    if (!v.isDouble())
        return new NumItem(QStringLiteral("—"), -1e9);
    return new NumItem(QString::number(v.toDouble(), 'f', decimals), v.toDouble());
}

} // namespace

StrategiesPanel::StrategiesPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    bar_ = new QWidget(this);
    auto* hb = new QHBoxLayout(bar_);
    hb->setContentsMargins(12, 6, 12, 6);
    hb->setSpacing(8);
    summary_lbl_ = new QLabel;
    summary_lbl_->setObjectName("strategiesSummary");
    summary_lbl_->setTextFormat(Qt::RichText);
    hb->addWidget(summary_lbl_, 1);
    refresh_btn_ = new QPushButton;
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &StrategiesPanel::load);
    hb->addWidget(refresh_btn_);
    root->addWidget(bar_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(ColCount);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(true);
    table_->setSortingEnabled(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(Decision, QHeaderView::Stretch);
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() {
        const QString m = selected_model();
        if (!m.isEmpty() && m != history_model_) {
            history_model_ = m;
            load_history();
        }
    });

    // Strategies on top, the selected strategy's daily history below
    auto* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    split->addWidget(table_);
    auto* hist_box = new QWidget(split);
    auto* hv = new QVBoxLayout(hist_box);
    hv->setContentsMargins(0, 0, 0, 0);
    hv->setSpacing(0);
    history_title_ = new QLabel(hist_box);
    history_title_->setObjectName("strategiesHistoryTitle");
    hv->addWidget(history_title_);
    history_ = new QTableWidget(hist_box);
    history_->setColumnCount(HColCount);
    history_->setSelectionBehavior(QAbstractItemView::SelectRows);
    history_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    history_->setShowGrid(false);
    history_->setAlternatingRowColors(true);
    history_->verticalHeader()->setVisible(false);
    history_->horizontalHeader()->setHighlightSections(false);
    history_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    history_->horizontalHeader()->setSectionResizeMode(HTrade, QHeaderView::Stretch);
    hv->addWidget(history_, 1);
    split->addWidget(hist_box);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    root->addWidget(split, 1);

    note_lbl_ = new QLabel;
    note_lbl_->setObjectName("strategiesNote");
    note_lbl_->setWordWrap(true);
    note_lbl_->setContentsMargins(12, 6, 12, 8);
    root->addWidget(note_lbl_);

    timer_ = new QTimer(this);
    timer_->setInterval(kStrategiesRefreshMs);
    connect(timer_, &QTimer::timeout, this, &StrategiesPanel::load);

    retranslate();
}

void StrategiesPanel::apply_styles() {
    if (bar_)
        bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    if (note_lbl_)
        note_lbl_->setStyleSheet(QString("color: %1; font-size: 10px;").arg(ui::colors::TEXT_TERTIARY()));
    if (summary_lbl_)
        summary_lbl_->setStyleSheet(QString("color: %1; font-size: 11px;").arg(ui::colors::TEXT_SECONDARY()));
    if (history_title_)
        history_title_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; padding: 8px 12px; "
                                              "background: %2; border-top: 1px solid %3; border-bottom: 1px solid %3;")
                                          .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_SURFACE(),
                                               ui::colors::BORDER_DIM()));
    for (QTableWidget* t : {table_, history_})
        if (t)
            t->setStyleSheet(
            QString("QTableWidget { background: %1; color: %2; border: none; gridline-color: %3; }")
                .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()) +
            QString("QTableWidget::item { padding: 6px 8px; }") +
            QString("QTableWidget::item:alternate { background: %1; }").arg(ui::colors::BG_RAISED()) +
            QString("QHeaderView::section { background: %1; color: %2; border: none; border-bottom: 1px solid %3; "
                    "padding: 6px 8px; font-size: 10px; font-weight: 700; }")
                .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_MED()));
}

void StrategiesPanel::retranslate() {
    history_->setHorizontalHeaderLabels({tr("DATE"), tr("DECISION"), tr("TQQQ"), tr("SQQQ"), tr("TRADE"),
                                         tr("HOLDING"), tr("VALUE"), tr("DAY"), tr("SINCE START")});
    if (history_model_.isEmpty())
        history_title_->setText(tr("DAILY HISTORY · select a strategy above"));
    refresh_btn_->setText(tr("REFRESH"));
    refresh_btn_->setAccessibleName(tr("Refresh strategy returns now"));
    table_->setHorizontalHeaderLabels({tr("STRATEGY"), tr("DECISION"), tr("TQQQ"), tr("SQQQ"), tr("LIVE RETURN"),
                                       tr("LAST DAY"), tr("LIVE MAX DD"), tr("DAYS"), tr("BT CAGR 2011+"),
                                       tr("BT MAX DD"), tr("BT SHARPE"), tr("BT LAST 12M")});
    table_->horizontalHeaderItem(LiveRet)->setToolTip(tr("Paper NAV since tracking started (rotation_daily)"));
    table_->horizontalHeaderItem(BtCagr)->setToolTip(tr("Backtest annual return since 2011 (rotation_backtest)"));
    note_lbl_->setText(tr("● = the model the engine trades. Live returns are paper NAVs: each day's target "
                          "allocation applied to the TQQQ/SQQQ price change, no costs. Backtests use the same "
                          "signals since 2011 with 5 bp costs; they are not a promise of future returns."));
    if (summary_lbl_->text().isEmpty())
        summary_lbl_->setText(tr("Loading strategies…"));
}

void StrategiesPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    load();
    timer_->start();
}

void StrategiesPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    timer_->stop();
}

QString StrategiesPanel::selected_model() const {
    const auto rows = table_->selectionModel() ? table_->selectionModel()->selectedRows() : QModelIndexList{};
    if (rows.isEmpty())
        return {};
    const QTableWidgetItem* item = table_->item(rows.first().row(), Model);
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void StrategiesPanel::load_history() {
    if (history_model_.isEmpty())
        return;
    const QString model = history_model_;
    const QString url = api_url("/rotation/history?capital=%1&model=")
                            .arg(static_cast<qint64>(kHistoryCapital)) +
                        QUrl::toPercentEncoding(model);
    HttpClient::instance().get(
        url,
        [this, model](Result<QJsonDocument> r) {
            if (model != history_model_)  // user moved on to another strategy
                return;
            if (r.is_err()) {
                history_title_->setText(tr("DAILY HISTORY · %1 · unavailable: %2")
                                            .arg(model, HttpClient::message_from_error(r.error())));
                return;
            }
            render_history(r.value().object());
        },
        this);
}

void StrategiesPanel::render_history(const QJsonObject& data) {
    const QJsonArray rows = data.value("rows").toArray();
    int trades = 0;
    for (const QJsonValue& v : rows)
        if (!v.toObject().value("trade").toString().isEmpty())
            ++trades;
    history_title_->setText(tr("DAILY HISTORY · %1 · %2 days · %3 trades · on $%4, shares as the engine would size them")
                                .arg(history_model_)
                                .arg(rows.size())
                                .arg(trades)
                                .arg(QLocale().toString(kHistoryCapital, 'f', 0)));
    history_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject h = rows.at(i).toObject();
        const QString trade = h.value("trade").toString();
        auto* trade_item = new QTableWidgetItem(trade.isEmpty() ? QStringLiteral("—") : trade);
        if (trade.isEmpty())
            trade_item->setForeground(QColor(ui::colors::TEXT_DIM()));
        else
            trade_item->setForeground(QColor(trade.startsWith("SELL") ? ui::colors::NEGATIVE()
                                                                      : ui::colors::POSITIVE()));
        QStringList holding;
        if (h.value("tqqq_shares").toInt() > 0)
            holding << QString("%1 TQQQ").arg(QLocale().toString(h.value("tqqq_shares").toInt()));
        if (h.value("sqqq_shares").toInt() > 0)
            holding << QString("%1 SQQQ").arg(QLocale().toString(h.value("sqqq_shares").toInt()));

        history_->setItem(i, HDate, new QTableWidgetItem(h.value("as_of").toString()));
        history_->setItem(i, HDecision, new QTableWidgetItem(h.value("decision").toString()));
        history_->setItem(i, HTqqq, pct(h.value("long_alloc"), false, 0));
        history_->setItem(i, HSqqq, pct(h.value("inverse_alloc"), false, 0));
        history_->setItem(i, HTrade, trade_item);
        history_->setItem(i, HHolding, new QTableWidgetItem(holding.isEmpty() ? tr("cash") : holding.join(", ")));
        history_->setItem(i, HValue, new NumItem("$" + QLocale().toString(h.value("value").toDouble(), 'f', 0),
                                                 h.value("value").toDouble()));
        history_->setItem(i, HDay, pct(h.value("day_return"), true, 2));
        history_->setItem(i, HTotal, pct(h.value("live_return"), true, 2));
    }
}

void StrategiesPanel::load() {
    HttpClient::instance().get(
        api_url("/rotation/strategies"),
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                summary_lbl_->setText(tr("Cannot reach %1: %2")
                                          .arg(api_base_url(), HttpClient::message_from_error(r.error()))
                                          .toHtmlEscaped());
                return;
            }
            render(r.value().object());
        },
        this);
}

void StrategiesPanel::render(const QJsonObject& data) {
    const QJsonArray rows = data.value("strategies").toArray();
    if (rows.isEmpty()) {
        summary_lbl_->setText(tr("No strategy results recorded yet."));
        table_->setRowCount(0);
        return;
    }

    // Header line: tracking period and the benchmark backtests for comparison
    const QJsonObject first = rows.first().toObject();
    QString summary = tr("Tracking since <b>%1</b> · last update <b>%2</b> · %3 strategies")
                          .arg(first.value("since").toString(), first.value("as_of").toString())
                          .arg(rows.size());
    const QJsonObject bench = data.value("benchmarks").toObject();
    for (auto it = bench.begin(); it != bench.end(); ++it) {
        const QJsonObject b = it.value().toObject().value("since_2011").toObject();
        if (b.value("cagr").isDouble())
            summary += QString(" · %1: %2%/yr, max DD %3%")
                           .arg(it.key().toHtmlEscaped())
                           .arg(b.value("cagr").toDouble() * 100, 0, 'f', 1)
                           .arg(b.value("max_drawdown").toDouble() * 100, 0, 'f', 0);
    }
    summary_lbl_->setText(summary);

    // Keep the user's sort column while repopulating
    const int sort_col = table_->horizontalHeader()->sortIndicatorSection();
    const Qt::SortOrder sort_order = table_->horizontalHeader()->sortIndicatorOrder();
    table_->setSortingEnabled(false);
    table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject s = rows.at(i).toObject();
        const bool active = s.value("active").toBool();
        const QJsonObject bt = s.value("backtest").toObject();
        const QJsonObject b11 = bt.value("since_2011").toObject();
        const QJsonObject b12 = bt.value("last_12m").toObject();

        auto* name = new QTableWidgetItem((active ? QStringLiteral("● ") : QStringLiteral("   ")) +
                                          s.value("model").toString());
        name->setData(Qt::UserRole, s.value("model").toString());
        if (active) {
            QFont f = name->font();
            f.setBold(true);
            name->setFont(f);
            name->setForeground(QColor(ui::colors::AMBER()));
            name->setToolTip(tr("Traded by the execution engine"));
        }
        table_->setItem(i, Model, name);
        table_->setItem(i, Decision, new QTableWidgetItem(s.value("decision").toString()));
        table_->setItem(i, Tqqq, pct(s.value("long_alloc"), false, 0));
        table_->setItem(i, Sqqq, pct(s.value("inverse_alloc"), false, 0));
        table_->setItem(i, LiveRet, pct(s.value("live_return"), true, 2));
        table_->setItem(i, DayRet, pct(s.value("day_return"), true, 2));
        table_->setItem(i, LiveDd, pct(s.value("live_max_drawdown"), true, 1));
        table_->setItem(i, Days, num(s.value("days"), 0));
        table_->setItem(i, BtCagr, pct(b11.value("cagr"), true));
        table_->setItem(i, BtDd, pct(b11.value("max_drawdown"), true, 0));
        table_->setItem(i, BtSharpe, num(b11.value("sharpe"), 2));
        table_->setItem(i, Bt12m, pct(b12.value("total_return"), true));
    }
    table_->setSortingEnabled(true);
    if (sort_col >= 0 && sort_col < ColCount && table_->horizontalHeader()->isSortIndicatorShown())
        table_->sortItems(sort_col, sort_order);

    // Keep the selected strategy selected across refreshes; start with the traded one
    QString want = history_model_;
    for (int i = 0; want.isEmpty() && i < rows.size(); ++i)
        if (rows.at(i).toObject().value("active").toBool())
            want = rows.at(i).toObject().value("model").toString();
    for (int row = 0; row < table_->rowCount(); ++row) {
        if (table_->item(row, Model) && table_->item(row, Model)->data(Qt::UserRole).toString() == want) {
            QSignalBlocker block(table_);
            table_->selectRow(row);
            break;
        }
    }
    history_model_ = want;
    load_history();
}

} // namespace fincept::autotrade
