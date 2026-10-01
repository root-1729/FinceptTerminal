#include "autotrade/AutotradeScreen.h"

#include "autotrade/AutotradeApi.h"
#include "autotrade/StrategiesPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::autotrade {

namespace {

constexpr int kRefreshMs = 10000;
// A screen with quotes takes ~5-20s; poll every 3s for up to a minute
constexpr int kScanPollMs = 3000;
constexpr int kScanMaxPolls = 20;

QString fmt_num(const QJsonValue& v, int decimals = 2) {
    if (!v.isDouble())
        return QStringLiteral("—");
    return QLocale().toString(v.toDouble(), 'f', decimals);
}

QString fmt_money(const QJsonValue& v) {
    return v.isDouble() ? QStringLiteral("$") + fmt_num(v) : QStringLiteral("—");
}

QString fmt_volume(const QJsonValue& v) {
    if (!v.isDouble())
        return QStringLiteral("—");
    const double d = v.toDouble();
    if (d >= 1e9)
        return QString("%1B").arg(d / 1e9, 0, 'f', 1);
    if (d >= 1e6)
        return QString("%1M").arg(d / 1e6, 0, 'f', 1);
    if (d >= 1e3)
        return QString("%1K").arg(d / 1e3, 0, 'f', 0);
    return QString::number(static_cast<long long>(d));
}

QTableWidgetItem* cell(const QString& text, bool numeric = false) {
    auto* item = new QTableWidgetItem(text);
    if (numeric)
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

/// Numeric cell coloured by sign (gains green, losses red).
QTableWidgetItem* signed_cell(const QJsonValue& v, const QString& text) {
    auto* item = cell(text, true);
    if (v.isDouble() && v.toDouble() != 0)
        item->setForeground(QColor(v.toDouble() > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    return item;
}

QTableWidget* make_table(QWidget* parent, int columns) {
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

QLabel* section_label(QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName("autotradeSection");
    return l;
}

} // namespace

AutotradeScreen::AutotradeScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    apply_styles();
    retranslate();

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(kRefreshMs);
    connect(refresh_timer_, &QTimer::timeout, this, &AutotradeScreen::refresh_now);

    scan_timer_ = new QTimer(this);
    scan_timer_->setInterval(kScanPollMs);
    connect(scan_timer_, &QTimer::timeout, this, &AutotradeScreen::poll_screen);
}

void AutotradeScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ──────────────────────────────────────────────────────────
    header_bar_ = new QWidget(this);
    auto* hb = new QHBoxLayout(header_bar_);
    hb->setContentsMargins(16, 12, 16, 12);
    hb->setSpacing(10);

    auto* title_box = new QVBoxLayout;
    title_box->setSpacing(1);
    title_lbl_ = new QLabel;
    title_lbl_->setObjectName("autotradeTitle");
    subtitle_lbl_ = new QLabel;
    subtitle_lbl_->setObjectName("autotradeSubtitle");
    title_box->addWidget(title_lbl_);
    title_box->addWidget(subtitle_lbl_);
    hb->addLayout(title_box);
    hb->addStretch();

    status_lbl_ = new QLabel;
    status_lbl_->setObjectName("autotradeStatus");
    hb->addWidget(status_lbl_);

    refresh_btn_ = new QPushButton;
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
        refresh_now();
        load_screener_results();
    });
    hb->addWidget(refresh_btn_);
    root->addWidget(header_bar_);

    // ── Account summary ─────────────────────────────────────────────────────
    summary_bar_ = new QWidget(this);
    auto* sb = new QHBoxLayout(summary_bar_);
    sb->setContentsMargins(16, 8, 16, 8);
    sb->setSpacing(24);
    for (QLabel** l : {&account_lbl_, &nlv_lbl_, &cash_lbl_, &pnl_lbl_}) {
        *l = new QLabel;
        (*l)->setObjectName("autotradeStat");
        (*l)->setTextFormat(Qt::RichText);
        sb->addWidget(*l);
    }
    sb->addStretch();
    root->addWidget(summary_bar_);

    // ── Tabs: overview (positions + orders | screener) and strategies ───────
    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    // The app's global style squeezes tab labels to "OVERV…"; never elide them
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    auto* splitter = new QSplitter(Qt::Horizontal, tabs_);
    splitter->setChildrenCollapsible(false);

    auto* left = new QWidget(splitter);
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(0);
    positions_title_ = section_label(left);
    lv->addWidget(positions_title_);
    positions_table_ = make_table(left, 6);
    lv->addWidget(positions_table_, 3);
    orders_title_ = section_label(left);
    lv->addWidget(orders_title_);
    orders_table_ = make_table(left, 7);
    lv->addWidget(orders_table_, 2);

    auto* right = new QWidget(splitter);
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);
    auto* scan_bar = new QWidget(right);
    scan_bar->setObjectName("autotradeScanBar");
    auto* scb = new QHBoxLayout(scan_bar);
    scb->setContentsMargins(12, 6, 12, 6);
    scb->setSpacing(8);
    screener_title_ = section_label(scan_bar);
    screener_title_->setContentsMargins(0, 0, 0, 0);
    scb->addWidget(screener_title_);
    config_combo_ = new QComboBox;
    config_combo_->setMinimumWidth(200);
    connect(config_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { load_screener_results(); });
    scb->addWidget(config_combo_);
    run_btn_ = new QPushButton;
    run_btn_->setObjectName("autotradeRun");
    run_btn_->setCursor(Qt::PointingHandCursor);
    connect(run_btn_, &QPushButton::clicked, this, &AutotradeScreen::run_screen);
    scb->addWidget(run_btn_);
    scan_lbl_ = new QLabel;
    scan_lbl_->setObjectName("autotradeSubtitle");
    scb->addWidget(scan_lbl_, 1);
    rv->addWidget(scan_bar);
    screener_table_ = make_table(right, 6);
    rv->addWidget(screener_table_, 1);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    tabs_->addTab(splitter, QString());
    strategies_ = new StrategiesPanel(tabs_);
    tabs_->addTab(strategies_, QString());
    root->addWidget(tabs_, 1);
}

void AutotradeScreen::apply_styles() {
    setStyleSheet(
        QString("AutotradeScreen { background: %1; }").arg(ui::colors::BG_BASE()) +
        QString("#autotradeTitle { color: %1; font-size: 16px; font-weight: 700; }").arg(ui::colors::TEXT_PRIMARY()) +
        QString("#autotradeSubtitle { color: %1; font-size: 11px; }").arg(ui::colors::TEXT_TERTIARY()) +
        QString("#autotradeStat { color: %1; font-size: 12px; }").arg(ui::colors::TEXT_SECONDARY()) +
        QString("#autotradeSection { color: %1; font-size: 10px; font-weight: 700; padding: 8px 12px; "
                "background: %2; border-bottom: 1px solid %3; }")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()) +
        QString("#autotradeScanBar { background: %1; border-bottom: 1px solid %2; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()) +
        QString("#autotradeScanBar #autotradeSection { border: none; padding: 0; }") +
        QString("QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: 3px; padding: 4px 8px; }"
                "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()) +
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 3px; padding: 4px 12px; }"
                "QPushButton:hover { border-color: %4; }"
                "QPushButton:disabled { color: %5; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::INFO(),
                 ui::colors::TEXT_DIM()) +
        QString("#autotradeRun { background: %1; color: %2; border: none; font-weight: 700; }"
                "#autotradeRun:disabled { background: %3; }")
            .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM()));

    if (header_bar_)
        header_bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                       .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    if (summary_bar_)
        summary_bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                        .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    const QString table_css =
        QString("QTableWidget { background: %1; color: %2; border: none; gridline-color: %3; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()) +
        QString("QTableWidget::item { padding: 6px 8px; }") +
        QString("QTableWidget::item:alternate { background: %1; }").arg(ui::colors::BG_RAISED()) +
        QString("QHeaderView::section { background: %1; color: %2; border: none; border-bottom: 1px solid %3; "
                "padding: 6px 8px; font-size: 10px; font-weight: 700; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_MED());
    for (QTableWidget* t : {positions_table_, orders_table_, screener_table_})
        if (t)
            t->setStyleSheet(table_css);

    if (tabs_)
        tabs_->setStyleSheet(
            QString("QTabWidget::pane { border: none; }"
                    "QTabBar::tab { background: %1; color: %2; padding: 8px 20px; min-width: 120px; border: none; "
                    "border-bottom: 2px solid transparent; font-size: 11px; font-weight: 700; }"
                    "QTabBar::tab:selected { color: %3; border-bottom-color: %4; }"
                    "QTabBar::tab:hover { color: %3; }")
                .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_PRIMARY(),
                     ui::colors::AMBER()));
    if (strategies_)
        strategies_->apply_styles();

    set_connected(connected_, QString());
}

void AutotradeScreen::retranslate() {
    title_lbl_->setText(tr("AUTOTRADE"));
    tabs_->setTabText(0, tr("OVERVIEW"));
    tabs_->setTabText(1, tr("STRATEGIES"));
    strategies_->retranslate();
    subtitle_lbl_->setText(tr("IBKR paper stack via %1").arg(api_base_url()));
    refresh_btn_->setText(tr("REFRESH"));
    refresh_btn_->setAccessibleName(tr("Refresh account, positions and orders now"));
    positions_title_->setText(tr("POSITIONS"));
    orders_title_->setText(tr("OPEN ORDERS"));
    screener_title_->setText(tr("SCREENER"));
    run_btn_->setText(tr("RUN SCAN"));
    run_btn_->setAccessibleName(tr("Run the selected screener now"));
    config_combo_->setAccessibleName(tr("Screener configuration"));

    positions_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("QTY"), tr("AVG PRICE"), tr("MKT VALUE"), tr("UNRL P&L"), tr("P&L %")});
    orders_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("SIDE"), tr("QTY"), tr("TYPE"), tr("PRICE"), tr("FILLED"), tr("STATUS")});
    screener_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("NAME"), tr("EXCH"), tr("PRICE"), tr("CHG%"), tr("VOLUME")});

    if (account_lbl_->text().isEmpty()) {
        const QString dash = QStringLiteral("—");
        account_lbl_->setText(tr("ACCOUNT %1").arg(dash));
        nlv_lbl_->setText(tr("NLV %1").arg(dash));
        cash_lbl_->setText(tr("CASH %1").arg(dash));
        pnl_lbl_->setText(tr("UNRL P&L %1").arg(dash));
    }
}

void AutotradeScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_now();
    if (configs_.isEmpty())
        load_screener_configs();
    refresh_timer_->start();
}

void AutotradeScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
}

void AutotradeScreen::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
    if (!e)
        return;
    if (e->type() == QEvent::StyleChange || e->type() == QEvent::PaletteChange) {
        // apply_styles() calls setStyleSheet(), which posts another StyleChange:
        // guard against recursing until the stack overflows (see ScreenerScreen)
        if (restyling_)
            return;
        restyling_ = true;
        apply_styles();
        restyling_ = false;
    } else if (e->type() == QEvent::LanguageChange) {
        retranslate();
    }
}

// ── Account data ────────────────────────────────────────────────────────────

void AutotradeScreen::refresh_now() {
    load_health();
    load_account();
    load_positions();
    load_orders();
}

void AutotradeScreen::load_health() {
    HttpClient::instance().get(
        api_url("/health"),
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                set_connected(false, QString::fromStdString(r.error()));
                return;
            }
            const QJsonObject services = r.value().object().value("services").toObject();
            QStringList down;
            for (auto it = services.begin(); it != services.end(); ++it)
                if (it.value().toString() != QLatin1String("healthy"))
                    down << it.key();
            set_connected(down.isEmpty(), down.isEmpty() ? QString() : tr("degraded: %1").arg(down.join(", ")));
        },
        this);
}

void AutotradeScreen::set_connected(bool ok, const QString& detail) {
    connected_ = ok;
    if (!status_lbl_)
        return;
    const QString text = ok ? tr("● CONNECTED") : tr("● OFFLINE");
    status_lbl_->setText(detail.isEmpty() ? text : text + "  " + detail);
    status_lbl_->setToolTip(api_base_url());
    status_lbl_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700;")
                                   .arg(ok ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
}

void AutotradeScreen::load_account() {
    HttpClient::instance().get(
        api_url("/account/summary"),
        [this](Result<QJsonDocument> r) {
            if (r.is_err())
                return;
            const QJsonObject a = r.value().object();
            const auto stat = [](const QString& label, const QString& value, const QString& color = QString()) {
                return QString("%1 <b style='color:%3'>%2</b>")
                    .arg(label.toHtmlEscaped(), value.toHtmlEscaped(),
                         color.isEmpty() ? QString(ui::colors::TEXT_PRIMARY()) : color);
            };
            account_lbl_->setText(stat(tr("ACCOUNT"), QString("%1 (%2)").arg(a.value("account_id").toString(),
                                                                          a.value("account_type").toString())));
            nlv_lbl_->setText(stat(tr("NLV"), fmt_money(a.value("net_liquidation_value"))));
            cash_lbl_->setText(stat(tr("CASH"), fmt_money(a.value("cash_balance"))));
            const QJsonValue pnl = a.value("unrealized_pnl");
            pnl_lbl_->setText(stat(tr("UNRL P&L"), fmt_money(pnl),
                                   pnl.toDouble() < 0 ? QString(ui::colors::NEGATIVE()) : QString(ui::colors::POSITIVE())));
        },
        this);
}

void AutotradeScreen::load_positions() {
    HttpClient::instance().get(
        api_url("/positions"),
        [this](Result<QJsonDocument> r) {
            if (r.is_ok())
                render_positions(r.value().object().value("positions").toArray());
        },
        this);
}

void AutotradeScreen::render_positions(const QJsonArray& rows) {
    positions_title_->setText(tr("POSITIONS (%1)").arg(rows.size()));
    positions_table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject p = rows.at(i).toObject();
        const double qty = p.value("quantity").toDouble();
        const double cost = std::abs(qty * p.value("avg_price").toDouble());
        const QJsonValue pnl = p.value("unrealized_pnl");
        const QJsonValue pnl_pct = cost > 0 && pnl.isDouble() ? QJsonValue(pnl.toDouble() / cost * 100) : QJsonValue();
        positions_table_->setItem(i, 0, cell(p.value("symbol").toString()));
        positions_table_->setItem(i, 1, cell(fmt_num(p.value("quantity"), 0), true));
        positions_table_->setItem(i, 2, cell(fmt_num(p.value("avg_price")), true));
        positions_table_->setItem(i, 3, cell(fmt_num(p.value("market_value")), true));
        positions_table_->setItem(i, 4, signed_cell(pnl, fmt_num(pnl)));
        positions_table_->setItem(i, 5, signed_cell(pnl_pct, pnl_pct.isDouble() ? fmt_num(pnl_pct) + "%" : "—"));
    }
}

void AutotradeScreen::load_orders() {
    HttpClient::instance().get(
        api_url("/orders"),
        [this](Result<QJsonDocument> r) {
            if (r.is_ok())
                render_orders(r.value().object().value("orders").toArray());
        },
        this);
}

void AutotradeScreen::render_orders(const QJsonArray& rows) {
    orders_title_->setText(tr("OPEN ORDERS (%1)").arg(rows.size()));
    orders_table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject o = rows.at(i).toObject();
        orders_table_->setItem(i, 0, cell(o.value("symbol").toString()));
        orders_table_->setItem(i, 1, cell(o.value("side").toString().toUpper()));
        orders_table_->setItem(i, 2, cell(fmt_num(o.value("quantity"), 0), true));
        orders_table_->setItem(i, 3, cell(o.value("order_type").toString().toUpper()));
        orders_table_->setItem(i, 4, cell(fmt_num(o.value("price")), true));
        orders_table_->setItem(i, 5, cell(fmt_num(o.value("filled_quantity"), 0), true));
        orders_table_->setItem(i, 6, cell(o.value("status").toString().toUpper()));
    }
}

// ── Screener ────────────────────────────────────────────────────────────────

void AutotradeScreen::load_screener_configs() {
    HttpClient::instance().get(
        api_url("/screener/configs"),
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                scan_lbl_->setText(tr("Screener unavailable"));
                return;
            }
            configs_.clear();
            for (const QJsonValue& v : r.value().object().value("configs").toArray()) {
                const QJsonObject c = v.toObject();
                configs_.append({c.value("name").toString(), c.value("filename").toString()});
            }
            QSignalBlocker block(config_combo_);
            config_combo_->clear();
            for (const auto& c : configs_)
                config_combo_->addItem(c.first);
            load_screener_results();
        },
        this);
}

void AutotradeScreen::load_screener_results() {
    const QString name = config_combo_->currentText();
    if (name.isEmpty())
        return;
    const QString url = api_url("/screener/latest?limit=50&config_name=") + QUrl::toPercentEncoding(name);
    HttpClient::instance().get(
        url,
        [this, name](Result<QJsonDocument> r) {
            // Ignore late replies for a config the user has since switched away from
            if (r.is_err() || name != config_combo_->currentText())
                return;
            const QJsonObject d = r.value().object();
            render_screener(d.value("session").toObject(), d.value("results").toArray());
        },
        this);
}

void AutotradeScreen::render_screener(const QJsonObject& session, const QJsonArray& rows) {
    last_session_id_ = session.value("session_id").toString();
    if (!scan_timer_->isActive()) {
        const QString when = session.value("completed_at").toString();
        scan_lbl_->setText(rows.isEmpty() ? tr("No results yet. Run a scan.")
                                          : tr("%1 results · %2").arg(rows.size()).arg(when.left(19).replace('T', ' ')));
    }
    // Keep the previous table on screen until there's something to replace it with
    if (rows.isEmpty() && scan_timer_->isActive())
        return;
    screener_table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject s = rows.at(i).toObject();
        const QJsonValue chg = s.value("change_percent");
        screener_table_->setItem(i, 0, cell(s.value("symbol").toString()));
        screener_table_->setItem(i, 1, cell(s.value("company_name").toString()));
        screener_table_->setItem(i, 2, cell(s.value("exchange").toString()));
        screener_table_->setItem(i, 3, cell(fmt_num(s.value("price"), 2), true));
        screener_table_->setItem(i, 4, signed_cell(chg, chg.isDouble() ? fmt_num(chg) + "%" : "—"));
        screener_table_->setItem(i, 5, cell(fmt_volume(s.value("volume")), true));
    }
}

void AutotradeScreen::run_screen() {
    const int idx = config_combo_->currentIndex();
    if (idx < 0 || idx >= configs_.size())
        return;
    QJsonObject body;
    body["config_name"] = configs_.at(idx).second;
    body["fetch_quotes"] = true;
    set_scanning(true);
    HttpClient::instance().post(
        api_url("/screener/run"), body,
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                set_scanning(false);
                scan_lbl_->setText(tr("Scan failed: %1").arg(HttpClient::message_from_error(r.error())));
                return;
            }
            // The screen runs in the background; poll until a new session appears
            scan_before_id_ = last_session_id_;
            scan_polls_left_ = kScanMaxPolls;
            scan_timer_->start();
        },
        this);
}

void AutotradeScreen::poll_screen() {
    if (--scan_polls_left_ < 0) {
        set_scanning(false);
        scan_lbl_->setText(tr("Scan is taking longer than expected. Press REFRESH later."));
        return;
    }
    const QString name = config_combo_->currentText();
    const QString url = api_url("/screener/latest?limit=50&config_name=") + QUrl::toPercentEncoding(name);
    HttpClient::instance().get(
        url,
        [this, name](Result<QJsonDocument> r) {
            if (r.is_err() || !scan_timer_->isActive() || name != config_combo_->currentText())
                return;
            const QJsonObject d = r.value().object();
            const QJsonObject session = d.value("session").toObject();
            const QString id = session.value("session_id").toString();
            if (id.isEmpty() || id == QLatin1String("none") || id == scan_before_id_)
                return;
            set_scanning(false);
            render_screener(session, d.value("results").toArray());
        },
        this);
}

void AutotradeScreen::set_scanning(bool scanning) {
    if (!scanning)
        scan_timer_->stop();
    run_btn_->setEnabled(!scanning);
    config_combo_->setEnabled(!scanning);
    if (scanning)
        scan_lbl_->setText(tr("Scanning…"));
}

} // namespace fincept::autotrade
