#pragma once
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

namespace fincept::autotrade {

/// Autotrade > Strategies: every TQQQ/SQQQ rotation model side by side, from the
/// api-gateway's /rotation/strategies. Live paper returns (NAV since tracking started)
/// next to backtest stats (since 2011 and last 12 months). Refreshes every minute
/// while visible.
class StrategiesPanel : public QWidget {
    Q_OBJECT
  public:
    explicit StrategiesPanel(QWidget* parent = nullptr);

    void apply_styles();
    void retranslate();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void load();
    void render(const QJsonObject& data);

    QLabel* summary_lbl_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* note_lbl_ = nullptr;
    QTimer* timer_ = nullptr;
    QWidget* bar_ = nullptr;
};

} // namespace fincept::autotrade
