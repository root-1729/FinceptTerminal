#include "autotrade/AccountsPanel.h"

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

constexpr int kAccountsRefreshMs = 30000;

QTableWidget* ac_table(QWidget* parent, const int columns) {
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

QLabel* ac_section(QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName("accountsSection");
    return l;
}

QTableWidgetItem* ac_txt(const QString& s) { return new QTableWidgetItem(s); }

QTableWidgetItem* ac_num(const QJsonValue& v, int decimals, bool signed_color = false) {
    auto* it = new QTableWidgetItem(v.isDouble() ? QLocale().toString(v.toDouble(), 'f', decimals) : QStringLiteral("—"));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (signed_color && v.isDouble() && v.toDouble() != 0)
        it->setForeground(QColor(v.toDouble() > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    return it;
}

QString ac_join(const QJsonArray& a) {
    QStringList out;
    for (const auto& v : a)
        out << v.toString();
    return out.isEmpty() ? QStringLiteral("—") : out.join(", ");
}

} // namespace

AccountsPanel::AccountsPanel(QWidget* parent) : QWidget(parent) {
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
    connect(refresh_btn_, &QPushButton::clicked, this, &AccountsPanel::load);
    hb->addWidget(refresh_btn_);
    root->addWidget(bar_);

    auto* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    auto box = [split](QLabel*& title, QTableWidget*& table, int cols) {
        auto* w = new QWidget(split);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);
        title = ac_section(w);
        v->addWidget(title);
        table = ac_table(w, cols);
        v->addWidget(table, 1);
        split->addWidget(w);
    };
    box(accounts_title_, accounts_, 9);
    box(positions_title_, positions_, 8);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    root->addWidget(split, 1);

    note_lbl_ = new QLabel(this);
    note_lbl_->setWordWrap(true);
    note_lbl_->setContentsMargins(12, 6, 12, 8);
    root->addWidget(note_lbl_);

    timer_ = new QTimer(this);
    timer_->setInterval(kAccountsRefreshMs);
    connect(timer_, &QTimer::timeout, this, &AccountsPanel::load);
    retranslate();
}

void AccountsPanel::apply_styles() {
    const QString section = QString("color: %1; font-size: 10px; font-weight: 700; padding: 8px 12px; background: %2; "
                                    "border-top: 1px solid %3; border-bottom: 1px solid %3;")
                                .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM());
    for (QLabel* l : {accounts_title_, positions_title_})
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
    for (QTableWidget* t : {accounts_, positions_})
        if (t)
            t->setStyleSheet(css);
}

void AccountsPanel::retranslate() {
    refresh_btn_->setText(tr("REFRESH"));
    accounts_title_->setText(tr("ACCOUNTS"));
    positions_title_->setText(tr("POSITIONS"));
    accounts_->setHorizontalHeaderLabels({tr("BROKER"), tr("ACCOUNT"), tr("MODE"), tr("NET LIQUIDATION"), tr("CASH"),
                                          tr("BUYING POWER"), tr("UNREALISED P&L"), tr("STRATEGIES"), tr("STATUS")});
    positions_->setHorizontalHeaderLabels({tr("ACCOUNT"), tr("SYMBOL"), tr("QUANTITY"), tr("AVG PRICE"),
                                           tr("MARKET VALUE"), tr("UNREALISED P&L"), tr("STRATEGY"), tr("UPDATED")});
    note_lbl_->setText(tr("Read-only. Positions belong to the strategy that lists the symbol in its registry entry; "
                          "an UNATTRIBUTED position is held by no strategy (the watchdog alerts on it). "
                          "Tastytrade and Alpaca appear once their adapters are built."));
    if (summary_lbl_->text().isEmpty())
        summary_lbl_->setText(tr("Loading…"));
}

void AccountsPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    load();
    timer_->start();
}

void AccountsPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    timer_->stop();
}

void AccountsPanel::load() {
    HttpClient::instance().get(
        api_url("/platform/accounts"),
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                summary_lbl_->setText(tr("Cannot reach %1").arg(api_base_url()).toHtmlEscaped());
                return;
            }
            render(r.value().object());
        },
        this);
}

void AccountsPanel::render(const QJsonObject& data) {
    const QJsonObject t = data.value("totals").toObject();
    const int orphans = t.value("unattributed").toInt();
    summary_lbl_->setText(tr("<b>ACCOUNTS</b> · net liquidation <b>$%1</b> · unrealised <b>$%2</b> · %3 positions%4")
                              .arg(QLocale().toString(t.value("net_liquidation").toDouble(), 'f', 0))
                              .arg(QLocale().toString(t.value("unrealized_pnl").toDouble(), 'f', 2))
                              .arg(t.value("positions").toInt())
                              .arg(orphans ? tr(" · <b style='color:%1'>%2 unattributed</b>")
                                                 .arg(QString(ui::colors::NEGATIVE()))
                                                 .arg(orphans)
                                           : QString()));

    const QJsonArray accts = data.value("accounts").toArray();
    accounts_->setRowCount(accts.size());
    QList<QPair<QString, QJsonObject>> rows;
    for (int i = 0; i < accts.size(); ++i) {
        const QJsonObject a = accts.at(i).toObject();
        const bool on = a.value("connected").toBool();
        accounts_->setItem(i, 0, ac_txt(a.value("broker").toString().toUpper()));
        accounts_->setItem(i, 1, ac_txt(a.value("account").toString()));
        auto* mode = ac_txt(a.value("mode").toString().toUpper());
        if (a.value("mode").toString() == "live")
            mode->setForeground(QColor(ui::colors::NEGATIVE()));
        accounts_->setItem(i, 2, mode);
        accounts_->setItem(i, 3, ac_num(a.value("net_liquidation"), 0));
        accounts_->setItem(i, 4, ac_num(a.value("cash"), 0));
        accounts_->setItem(i, 5, ac_num(a.value("buying_power"), 0));
        accounts_->setItem(i, 6, ac_num(a.value("unrealized_pnl"), 2, true));
        accounts_->setItem(i, 7, ac_txt(ac_join(a.value("strategies").toArray())));
        auto* status = ac_txt(on ? tr("connected") : a.value("reason").toString());
        status->setForeground(QColor(on ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY()));
        accounts_->setItem(i, 8, status);
        for (const auto& p : a.value("positions").toArray())
            rows.append({a.value("account").toString(), p.toObject()});
    }

    positions_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject p = rows.at(i).second;
        positions_->setItem(i, 0, ac_txt(rows.at(i).first));
        positions_->setItem(i, 1, ac_txt(p.value("symbol").toString()));
        positions_->setItem(i, 2, ac_num(p.value("quantity"), 0, true));
        positions_->setItem(i, 3, ac_num(p.value("avg_price"), 2));
        positions_->setItem(i, 4, ac_num(p.value("market_value"), 2));
        positions_->setItem(i, 5, ac_num(p.value("unrealized_pnl"), 2, true));
        auto* owner = ac_txt(p.value("attributed").toBool() ? ac_join(p.value("strategies").toArray()) : tr("UNATTRIBUTED"));
        if (!p.value("attributed").toBool())
            owner->setForeground(QColor(ui::colors::NEGATIVE()));
        positions_->setItem(i, 6, owner);
        positions_->setItem(i, 7, ac_txt(p.value("updated_at").toString().left(19).replace('T', ' ')));
    }
    positions_title_->setText(tr("POSITIONS · %1").arg(rows.size()));
}

} // namespace fincept::autotrade
