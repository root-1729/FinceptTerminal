#pragma once
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

namespace fincept::autotrade {

/// Autotrade > Accounts: every broker account the strategies use, from the api-gateway's
/// /platform/accounts. One row per account (broker, mode, net liquidation, cash, buying power,
/// unrealised P&L, strategies using it) and every position with the strategy it belongs to;
/// positions no strategy claims are highlighted. Read-only. Refreshes every 30 seconds while visible.
class AccountsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AccountsPanel(QWidget* parent = nullptr);

    void apply_styles();
    void retranslate();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void load();
    void render(const QJsonObject& data);

    QWidget* bar_ = nullptr;
    QLabel* summary_lbl_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* accounts_title_ = nullptr;
    QTableWidget* accounts_ = nullptr;
    QLabel* positions_title_ = nullptr;
    QTableWidget* positions_ = nullptr;
    QLabel* note_lbl_ = nullptr;
    QTimer* timer_ = nullptr;
};

} // namespace fincept::autotrade
