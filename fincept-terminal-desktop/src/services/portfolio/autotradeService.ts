/**
 * Autotrade Portfolio Service
 *
 * Bridge between Autotrade broker adapter and Fincept's portfolio system
 * Fetches portfolio data from Autotrade via the adapter and transforms it
 * to Fincept's portfolio holding format
 */

import type { PortfolioSummary, HoldingWithQuote, Portfolio } from './portfolioService';
import type { AutotradePortfolioSummary, AutotradePortfolioHolding } from '../../brokers/autotrade';
import { transformAutotradePortfolioToFincept } from '../../brokers/autotrade';
import type { IStockBrokerAdapter } from '../../brokers/stocks/types';

// ============================================================================
// AUTOTRADE PORTFOLIO SERVICE
// ============================================================================

/**
 * Service to manage Autotrade portfolio data
 * Integrates with Fincept's portfolio system
 */
export class AutotradePortfolioService {
  private brokerAdapter: IStockBrokerAdapter | null = null;
  private defaultPortfolioId = 'autotrade-portfolio';

  /**
   * Set the broker adapter for fetching data
   */
  setBrokerAdapter(adapter: IStockBrokerAdapter): void {
    this.brokerAdapter = adapter;
  }

  /**
   * Get Autotrade portfolio summary
   * Fetches data from Autotrade and transforms to Fincept format
   */
  async getPortfolioSummary(accountId: string): Promise<PortfolioSummary> {
    if (!this.brokerAdapter) {
      throw new Error('Broker adapter not set. Call setBrokerAdapter() first.');
    }

    // Get portfolio data from Autotrade
    const autotradePortfolio = await this.fetchAutotradePortfolio(accountId);

    // Transform to Fincept format
    const finceptPortfolio = transformAutotradePortfolioToFincept(
      autotradePortfolio,
      this.defaultPortfolioId
    );

    // Build holdings with quote data (current prices from Autotrade)
    const holdings: HoldingWithQuote[] = finceptPortfolio.holdings.map((h: AutotradePortfolioHolding) => ({
      id: h.instrument_id,
      portfolio_id: this.defaultPortfolioId,
      symbol: h.symbol,
      quantity: h.quantity,
      avg_buy_price: h.avg_buy_price,
      current_price: h.current_price,
      market_value: h.market_value,
      cost_basis: h.cost_basis,
      unrealized_pnl: h.unrealized_pnl,
      unrealized_pnl_percent: h.unrealized_pnl_percent,
      day_change: h.day_change,
      day_change_percent: h.day_change_percent,
      weight: h.weight,
      first_purchase_date: h.first_purchase_date,
      last_updated: h.last_updated,
    }));

    // Get portfolio info (create a default portfolio entry)
    const portfolio: Portfolio = {
      id: this.defaultPortfolioId,
      name: 'Autotrade Portfolio',
      owner: accountId,
      currency: 'USD',
      description: 'Autotrade automated trading portfolio',
      created_at: autotradePortfolio.last_updated,
      updated_at: autotradePortfolio.last_updated,
    };

    return {
      portfolio,
      holdings,
      total_market_value: finceptPortfolio.totalMarketValue,
      total_cost_basis: finceptPortfolio.totalCostBasis,
      total_unrealized_pnl: finceptPortfolio.totalUnrealizedPnl,
      total_unrealized_pnl_percent: finceptPortfolio.totalUnrealizedPnlPercent,
      total_positions: finceptPortfolio.totalPositions,
      total_day_change: 0, // Autotrade doesn't provide day change at portfolio level
      total_day_change_percent: 0,
      last_updated: finceptPortfolio.lastUpdated,
    };
  }

  /**
   * Fetch portfolio data from Autotrade
   */
  private async fetchAutotradePortfolio(accountId: string): Promise<AutotradePortfolioSummary> {
    if (!this.brokerAdapter) {
      throw new Error('Broker adapter not set');
    }

    try {
      // Get funds which includes portfolio summary via Autotrade adapter
      const funds = await this.brokerAdapter.getFunds();

      // Get positions to build the full portfolio
      const positions = await this.brokerAdapter.getPositions();

      // Build the portfolio summary
      const totalMarketValue = positions.reduce((sum: number, p: any) => {
        return sum + Math.abs(p.quantity * p.lastPrice);
      }, 0);

      const totalCostBasis = positions.reduce((sum: number, p: any) => {
        return sum + Math.abs(p.quantity * p.averagePrice);
      }, 0);

      const totalUnrealizedPnl = positions.reduce((sum: number, p: any) => {
        return sum + p.pnl;
      }, 0);

      const portfolio: AutotradePortfolioSummary = {
        account_id: accountId,
        timestamp: Date.now(),
        currency: 'USD',
        cash_balance: funds.availableCash,
        total_market_value: totalMarketValue,
        total_cost_basis: totalCostBasis,
        total_unrealized_pnl: totalUnrealizedPnl,
        total_unrealized_pnl_percent: totalCostBasis > 0 ? (totalUnrealizedPnl / totalCostBasis) * 100 : 0,
        total_realized_pnl: 0, // Not available from current adapter
        total_positions: positions.length,
        net_liquidation_value: totalMarketValue + funds.availableCash,
        positions: positions.map((p: any) => ({
          symbol: p.symbol,
          instrument_id: `${p.symbol}.${p.exchange}`,
          exchange: p.exchange,
          productType: p.productType,
          quantity: p.quantity,
          buyQuantity: p.buyQuantity,
          sellQuantity: p.sellQuantity,
          buyValue: p.buyValue,
          sellValue: p.sellValue,
          averagePrice: p.averagePrice,
          lastPrice: p.lastPrice,
          pnl: p.pnl,
          pnlPercent: p.pnlPercent,
          dayPnl: p.dayPnl,
          overnight: p.overnight || true,
          market_value: Math.abs(p.quantity * p.lastPrice),
          cost_basis: Math.abs(p.quantity * p.averagePrice),
          day_change: p.dayPnl,
          day_change_percent: 0, // Calculate from dayPnl / costBasis if needed
          weight: 0, // Will be calculated in transform
          instrument_type: 'ETF' as any, // Default to ETF, could be enhanced
        })),
        last_updated: new Date().toISOString(),
      };

      return portfolio;
    } catch (error) {
      console.error('[AutotradePortfolioService] Failed to fetch portfolio:', error);
      throw error;
    }
  }

  /**
   * Get holdings with current prices
   * Fetches from Autotrade and formats for portfolio display
   */
  async getHoldings(accountId: string): Promise<HoldingWithQuote[]> {
    const summary = await this.getPortfolioSummary(accountId);
    return summary.holdings;
  }

  /**
   * Get Autotrade portfolio metadata
   */
  getPortfolioId(): string {
    return this.defaultPortfolioId;
  }

  /**
   * Get portfolio name
   */
  getPortfolioName(): string {
    return 'Autotrade Portfolio';
  }
}

// Export singleton instance
export const autotradePortfolioService = new AutotradePortfolioService();
