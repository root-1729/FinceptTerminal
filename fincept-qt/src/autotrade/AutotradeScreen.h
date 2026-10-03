#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTimer;

namespace fincept::autotrade {

class ConditionsPanel;
class AccountsPanel;
class PlatformStrategiesPanel;
class StrategiesPanel;

/// Autotrade dashboard: live view of the autotrade stack through its api-gateway
/// (see AutotradeApi.h for the URL). Shows service health, the IB account summary,
/// open positions and orders, and runs the stock screener.
///
/// The api-gateway is read-only apart from screening, so there is no order entry.
/// Account data refreshes every 10s, only while the screen is visible.
class AutotradeScreen : public QWidget {
    Q_OBJECT
  public:
    explicit AutotradeScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void build_ui();
    void apply_styles();
    void retranslate();

    void refresh_now();
    void load_health();
    void load_account();
    void load_positions();
    void load_orders();

    void load_screener_configs();
    void load_screener_results();
    void run_screen();
    void poll_screen();
    void set_scanning(bool scanning);

    void render_positions(const QJsonArray& rows);
    void render_orders(const QJsonArray& rows);
    void render_screener(const QJsonObject& session, const QJsonArray& rows);
    void set_connected(bool ok, const QString& detail);

    QWidget* header_bar_ = nullptr;
    QLabel* title_lbl_ = nullptr;
    QLabel* subtitle_lbl_ = nullptr;
    QLabel* status_lbl_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    QWidget* summary_bar_ = nullptr;
    QLabel* account_lbl_ = nullptr;
    QLabel* nlv_lbl_ = nullptr;
    QLabel* cash_lbl_ = nullptr;
    QLabel* pnl_lbl_ = nullptr;

    QLabel* positions_title_ = nullptr;
    QTableWidget* positions_table_ = nullptr;
    QLabel* orders_title_ = nullptr;
    QTableWidget* orders_table_ = nullptr;

    QLabel* screener_title_ = nullptr;
    QComboBox* config_combo_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QLabel* scan_lbl_ = nullptr;
    QTableWidget* screener_table_ = nullptr;

    QTimer* refresh_timer_ = nullptr;
    QTimer* scan_timer_ = nullptr;

    // Screener config: display name → filename expected by /screener/run
    QList<QPair<QString, QString>> configs_;
    QString last_session_id_;
    QString scan_before_id_;
    int scan_polls_left_ = 0;
    bool connected_ = false;
    QTabWidget* tabs_ = nullptr;
    PlatformStrategiesPanel* platform_ = nullptr;
    AccountsPanel* accounts_ = nullptr;
    StrategiesPanel* strategies_ = nullptr;
    ConditionsPanel* conditions_ = nullptr;
    bool restyling_ = false;
};

} // namespace fincept::autotrade
