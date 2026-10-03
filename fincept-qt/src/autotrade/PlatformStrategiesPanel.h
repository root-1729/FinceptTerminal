#pragma once
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTimer;

namespace fincept::autotrade {

/// Autotrade > Strategies: every strategy in the autotrade registry, from the api-gateway's
/// /platform/strategies (registry + unified journal). One row per strategy: stage, whether it may
/// send orders, broker and account, live vs backtest return, drawdown, latest decision, and for a
/// shadow strategy how many days it matched the live one. Selecting a row loads
/// /platform/strategies/{id}: stage history, daily results next to the backtest, the latest day's
/// decisions and the fills. Refreshes every 30 seconds while visible.
class PlatformStrategiesPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PlatformStrategiesPanel(QWidget* parent = nullptr);

    void apply_styles();
    void retranslate();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void load();
    void load_detail();
    void render_list(const QJsonObject& data);
    void render_detail(const QJsonObject& data);

    QWidget* bar_ = nullptr;
    QLabel* summary_lbl_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QTableWidget* list_ = nullptr;
    QLabel* detail_lbl_ = nullptr;
    QTabWidget* detail_tabs_ = nullptr;
    QTableWidget* daily_ = nullptr;
    QTableWidget* decisions_ = nullptr;
    QTableWidget* fills_ = nullptr;
    QTableWidget* history_ = nullptr;
    QLabel* note_lbl_ = nullptr;
    QTimer* timer_ = nullptr;
    QString selected_;
};

} // namespace fincept::autotrade
