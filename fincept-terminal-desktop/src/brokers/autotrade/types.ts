/**
 * Autotrade Broker Types
 *
 * Types specific to the Autotrade automated trading system
 * Maps to the Autotrade Integration Service API (port 8001)
 */

import type {
  Position,
  Holding,
  Order,
  Quote,
  Instrument,
  StockExchange,
  Currency,
  Region,
  ProductType,
  InstrumentType,
} from '../stocks/types';

// ============================================================================
// AUTOTRADE-SPECIFIC TYPES
// ============================================================================

/**
 * Autotrade Account Information
 */
export interface AutotradeAccount {
  accountId: string;
  accountType: string;
  currency: Currency;
  status: 'ACTIVE' | 'INACTIVE' | 'SUSPENDED';
  createdDate: string;
  paperTrading: boolean;
}

/**
 * Autotrade Position - matches /api/v1/positions response
 * Extended from Position with autotrade-specific fields
 */
export interface AutotradePosition extends Position {
  instrument_id: string;              // Autotrade internal instrument ID
  market_value: number;               // Current market value
  cost_basis: number;                 // Total cost basis
  day_change: number;                 // Day's change in value
  day_change_percent: number;         // Day's change percentage
  weight: number;                     // Portfolio weight (0-1)
  instrument_type?: InstrumentType;   // Optional instrument type
}

/**
 * Autotrade Holding - similar to Position but for portfolio display
 * Used for Portfolio tab integration
 */
export interface AutotradeHolding extends Holding {
  instrument_id: string;              // Autotrade internal ID
  first_purchase_date: string;        // Date of first purchase
}

/**
 * Autotrade Portfolio Summary - matches /api/v1/portfolio response
 */
export interface AutotradePortfolioSummary {
  account_id: string;
  timestamp: number;
  currency: Currency;
  cash_balance: number;
  total_market_value: number;
  total_cost_basis: number;
  total_unrealized_pnl: number;
  total_unrealized_pnl_percent: number;
  total_realized_pnl: number;
  total_positions: number;
  net_liquidation_value: number;
  buying_power?: number;
  day_trading_buying_power?: number;
  positions: AutotradePosition[];
  last_updated: string;
}

/**
 * Autotrade Performance Data - matches /api/v1/portfolio/performance response
 */
export interface AutotradePerformanceSeries {
  timestamp: number;
  nav: number;                        // Net Asset Value
  cumulative_return: number;          // Cumulative return
  daily_return?: number;              // Daily return (optional)
}

export interface AutotradePerformance {
  currency: Currency;
  period: string;                     // "1d", "7d", "30d", "ytd", "1y", "all"
  series: AutotradePerformanceSeries[];
  annualized_return?: number;
  annualized_volatility?: number;
  sharpe_ratio?: number;
  max_drawdown?: number;
  sortino_ratio?: number;
}

/**
 * Autotrade Order - matches /api/v1/orders response
 */
export interface AutotradeOrder extends Order {
  instrument_id: string;              // Autotrade internal ID
  account_id: string;
  placed_by: 'AUTO' | 'MANUAL' | 'SYSTEM';
  strategy_id?: string;               // Strategy that placed the order
  tags?: string[];                    // Order tags for categorization
}

/**
 * Autotrade Order Response
 */
export interface AutotradeOrderResponse {
  success: boolean;
  order_id?: string;
  message?: string;
  errorCode?: string;
}

/**
 * Autotrade Market Data Quote
 */
export interface AutotradeQuote extends Quote {
  instrument_id: string;
  bid_size?: number;
  ask_size?: number;
  volume: number;                     // Required in Quote interface
  open_interest?: number;
}

/**
 * Autotrade Instrument
 */
export interface AutotradeInstrument extends Instrument {
  instrument_id: string;              // Autotrade internal ID
  is_enabled: boolean;
  category?: string;                  // e.g., "ETF", "STOCK", "INDEX"
}

// ============================================================================
// PORTFOLIO TRANSFORMATION TYPES
// ============================================================================

/**
 * Holding transformed for Fincept Portfolio system
 * Matches the format expected by PortfolioSummary
 */
export interface AutotradePortfolioHolding {
  symbol: string;
  instrument_id: string;
  portfolio_id: string;
  quantity: number;
  avg_buy_price: number;
  current_price: number;
  market_value: number;
  cost_basis: number;
  unrealized_pnl: number;
  unrealized_pnl_percent: number;
  day_change: number;
  day_change_percent: number;
  weight: number;
  first_purchase_date: string;
  last_updated: string;
}

// ============================================================================
// API RESPONSE TYPES
// ============================================================================

/**
 * Unified Autotrade API Response
 */
export interface AutotradeApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
  errorCode?: string;
  timestamp: number;
  stale?: boolean;                    // True if data is older than threshold
  age_seconds?: number;               // Age of data in seconds
}

/**
 * Positions API Response
 */
export interface AutotradePositionsResponse extends AutotradeApiResponse<AutotradePosition[]> {
  total_positions?: number;
}

/**
 * Portfolio Summary API Response
 */
export interface AutotradePortfolioResponse extends AutotradeApiResponse<AutotradePortfolioSummary> {
  // Additional metadata
  last_update_source?: 'API' | 'WebSocket' | 'Cache';
}

// ============================================================================
// TEST UTILITIES
// ============================================================================

/**
 * Sample Autotrade data for testing
 */
export const sampleAutotradePosition: AutotradePosition = {
  symbol: 'TQQQ',
  instrument_id: 'TQQQ.NASDAQ',
  exchange: 'NASDAQ',
  productType: 'CASH',
  quantity: 100,
  buyQuantity: 100,
  sellQuantity: 0,
  buyValue: 4550.0,
  sellValue: 0,
  averagePrice: 45.5,
  lastPrice: 46.25,
  pnl: 75.0,
  pnlPercent: 1.648,
  dayPnl: 12.5,
  overnight: true,
  market_value: 4625.0,
  cost_basis: 4550.0,
  day_change: 12.5,
  day_change_percent: 0.272,
  weight: 0.0231,
  instrument_type: 'ETF',
};

export const sampleAutotradePortfolio: AutotradePortfolioSummary = {
  account_id: 'DU8489265',
  timestamp: Date.now(),
  currency: 'USD',
  cash_balance: 50000.0,
  total_market_value: 200500.0,
  total_cost_basis: 180000.0,
  total_unrealized_pnl: 20500.0,
  total_unrealized_pnl_percent: 11.39,
  total_realized_pnl: 15000.0,
  total_positions: 8,
  net_liquidation_value: 250500.0,
  positions: [sampleAutotradePosition],
  last_updated: new Date().toISOString(),
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Transform AutotradePosition to Fincept Position
 */
export function transformAutotradePositionToPosition(
  autotradePosition: AutotradePosition,
): Position {
  return {
    symbol: autotradePosition.symbol,
    exchange: autotradePosition.exchange,
    productType: autotradePosition.productType || 'CASH',
    quantity: autotradePosition.quantity,
    buyQuantity: autotradePosition.quantity > 0 ? autotradePosition.quantity : 0,
    sellQuantity: autotradePosition.quantity < 0 ? Math.abs(autotradePosition.quantity) : 0,
    buyValue: autotradePosition.buyValue,
    sellValue: autotradePosition.sellValue,
    averagePrice: autotradePosition.averagePrice,
    lastPrice: autotradePosition.lastPrice,
    pnl: autotradePosition.pnl,
    pnlPercent: autotradePosition.pnlPercent,
    dayPnl: autotradePosition.dayPnl,
    overnight: true, // Autotrade positions are typically carried forward
  };
}

/**
 * Transform AutotradePortfolioSummary to Fincept PortfolioSummary
 * This is called by the portfolio service
 */
export function transformAutotradePortfolioToFincept(
  portfolio: AutotradePortfolioSummary,
  portfolioId: string,
): {
  holdings: AutotradePortfolioHolding[];
  totalMarketValue: number;
  totalCostBasis: number;
  totalUnrealizedPnl: number;
  totalUnrealizedPnlPercent: number;
  totalPositions: number;
  lastUpdated: string;
} {
  const holdings: AutotradePortfolioHolding[] = portfolio.positions.map((pos) => ({
    symbol: pos.symbol,
    instrument_id: pos.instrument_id,
    portfolio_id: portfolioId,
    quantity: pos.quantity,
    avg_buy_price: pos.averagePrice,
    current_price: pos.lastPrice,
    market_value: pos.market_value,
    cost_basis: pos.cost_basis,
    unrealized_pnl: pos.pnl,
    unrealized_pnl_percent: pos.pnlPercent,
    day_change: pos.dayPnl,
    day_change_percent: 0, // Calculate from pos.day_change_percent
    weight: pos.weight,
    first_purchase_date: new Date(Date.now() - 86400000).toISOString(), // Fallback: yesterday
    last_updated: portfolio.last_updated,
  }));

  return {
    holdings,
    totalMarketValue: portfolio.total_market_value,
    totalCostBasis: portfolio.total_cost_basis,
    totalUnrealizedPnl: portfolio.total_unrealized_pnl,
    totalUnrealizedPnlPercent: portfolio.total_unrealized_pnl_percent,
    totalPositions: portfolio.total_positions,
    lastUpdated: portfolio.last_updated,
  };
}
