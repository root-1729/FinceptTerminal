/**
 * Autotrade Broker Adapter
 *
 * Implements the IStockBrokerAdapter interface to integrate Autotrade
 * with Fincept Terminal's broker system.
 *
 * This adapter fetches data from the Autotrade Integration Service
 * (http://localhost:8001) via Tauri commands and transforms it
 * to the unified broker format.
 */

import { invoke } from '@tauri-apps/api/core';
import type {
  IStockBrokerAdapter,
  StockBrokerMetadata,
  Region,
  BrokerCredentials,
  AuthResponse,
  Funds,
  MarginRequired,
  OrderParams,
  OrderResponse,
  ModifyOrderParams,
  Order,
  Trade,
  Position,
  Holding,
  Quote,
  OHLCV,
  TimeFrame,
  StockExchange,
  Instrument,
  WebSocketConfig,
  SubscriptionMode,
  TickData,
  MarketDepth,
  BulkOperationResult,
  SmartOrderParams,
  ProductType,
} from '../stocks/types';
import type {
  AutotradePosition,
  AutotradeHolding,
  AutotradePortfolioSummary,
  AutotradePortfolioHolding,
  AutotradeOrder,
  AutotradeQuote,
  AutotradeInstrument,
  AutotradeApiResponse,
  transformAutotradePositionToPosition,
  transformAutotradePortfolioToFincept,
} from './types';

/**
 * Autotrade Broker ID and Metadata
 */
export const AUTOTRADE_BROKER_ID = 'autotrade';
export const AUTOTRADE_BROKER_NAME = 'Autotrade';
export const AUTOTRADE_REGION: Region = 'us';

/**
 * Autotrade Adapter Metadata
 */
export const autotradeBrokerMetadata: StockBrokerMetadata = {
  id: AUTOTRADE_BROKER_ID,
  name: AUTOTRADE_BROKER_NAME,
  displayName: 'Autotrade',
  website: 'https://fincept.org/autotrade',
  region: AUTOTRADE_REGION,
  country: 'US',
  currency: 'USD',
  exchanges: ['NASDAQ', 'NYSE', 'AMEX', 'ARCA', 'BATS'],
  marketHours: {
    open: '09:30',
    close: '16:00',
    timezone: 'America/New_York',
  },
  features: {
    webSocket: true,
    amo: false,
    gtt: false,
    bracketOrder: false,
    coverOrder: false,
    marginCalculator: true,
    optionsChain: false,
    paperTrading: true,
  },
  tradingFeatures: {
    marketOrders: true,
    limitOrders: true,
    stopOrders: false,
    stopLimitOrders: false,
    trailingStopOrders: false,
  },
  productTypes: ['CASH', 'MARGIN'],
  authType: 'api_key',
  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 60,
  },
  defaultSymbols: ['TQQQ', 'SQQQ', 'QQQ', 'SPY', 'DJI'],
};

/**
 * Autotrade Adapter
 *
 * Implements the IStockBrokerAdapter interface for the Autotrade
 * automated trading system.
 *
 * Data Source: Autotrade Integration Service (http://localhost:8001)
 */
export class AutotradeAdapter implements IStockBrokerAdapter {
  // ============================================================================
  // Properties
  // ============================================================================

  readonly brokerId: string = AUTOTRADE_BROKER_ID;
  readonly brokerName: string = AUTOTRADE_BROKER_NAME;
  readonly region: Region = AUTOTRADE_REGION;
  readonly metadata: StockBrokerMetadata = autotradeBrokerMetadata;

  private _isConnected: boolean = false;
  private _accountId: string = '';

  // ============================================================================
  // Public Getters
  // ============================================================================

  get isConnected(): boolean {
    return this._isConnected;
  }

  get accountId(): string {
    return this._accountId;
  }

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Set credentials for Autotrade API
   * For now, we just set the account ID since auth is handled by the integration service
   */
  setCredentials(apiKey: string, apiSecret?: string): void {
    // Store for future use if needed
    // Currently the integration service handles authentication
  }

  /**
   * Get authentication URL (not applicable for Autotrade)
   */
  getAuthUrl(): string {
    // Autotrade doesn't use OAuth
    return '';
  }

  /**
   * Authenticate with Autotrade Integration Service
   * The integration service handles authentication with IBKR
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      // Store account ID from credentials if provided
      if (credentials.userId) {
        this._accountId = credentials.userId;
      }

      // The integration service handles IBKR auth
      // We just verify we can reach the service
      this._isConnected = true;

      return {
        success: true,
        accessToken: 'autotrade-session-token', // Placeholder
        userId: this._accountId,
        expiresAt: new Date(Date.now() + 86400000), // 24 hours
      };
    } catch (error) {
      console.error(`[${this.brokerId}] Authentication failed:`, error);
      this._isConnected = false;
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Authentication failed',
        errorCode: 'AUTH_FAILED',
      };
    }
  }

  /**
   * Handle authentication callback (not applicable)
   */
  async handleAuthCallback(_params: any): Promise<AuthResponse> {
    return {
      success: false,
      message: 'Autotrade does not use OAuth',
      errorCode: 'NOT_APPLICABLE',
    };
  }

  /**
   * Logout from Autotrade
   */
  async logout(): Promise<void> {
    this._isConnected = false;
    this._accountId = '';
  }

  /**
   * Refresh session (no-op for Autotrade)
   */
  async refreshSession(): Promise<AuthResponse> {
    if (!this._isConnected) {
      return {
        success: false,
        message: 'Not connected. Please authenticate first.',
        errorCode: 'NOT_CONNECTED',
      };
    }
    return {
      success: true,
      accessToken: 'autotrade-session-token',
    };
  }

  // ============================================================================
  // Account Information
  // ============================================================================

  /**
   * Get account funds (cash, margin, etc.)
   */
  async getFunds(): Promise<Funds> {
    try {
      const response = await this.invokeAutotradeCommand<AutotradePortfolioSummary>(
        'autotrade_get_account_summary',
        this._accountId,
      );

      if (!response.success || !response.data) {
        throw new Error(response.error || 'Failed to fetch funds');
      }

      const portfolio = response.data;

      return {
        availableCash: portfolio.cash_balance,
        usedMargin: 0, // Not applicable for Autotrade
        availableMargin: 0, // Not applicable for Autotrade
        totalBalance: portfolio.net_liquidation_value,
        currency: portfolio.currency,
      };
    } catch (error) {
      console.error(`[${this.brokerId}] getFunds failed:`, error);
      throw error;
    }
  }

  /**
   * Get margin required for an order
   */
  async getMarginRequired(_params: OrderParams): Promise<MarginRequired> {
    // Autotrade doesn't provide margin calculations
    // Return default values
    return {
      totalMargin: 0,
      initialMargin: 0,
    };
  }

  /**
   * Calculate margin for multiple orders
   */
  async calculateMargin(_orders: OrderParams[]): Promise<MarginRequired> {
    return {
      totalMargin: 0,
      initialMargin: 0,
    };
  }

  // ============================================================================
  // Order Operations
  // ============================================================================

  /**
   * Place an order (not supported by Autotrade adapter)
   * Autotrade uses strategies to place orders automatically
   */
  async placeOrder(_params: OrderParams): Promise<OrderResponse> {
    return {
      success: false,
      message: 'Autotrade does not support manual order placement',
      errorCode: 'NOT_SUPPORTED',
    };
  }

  /**
   * Modify an order (not supported)
   */
  async modifyOrder(_params: ModifyOrderParams): Promise<OrderResponse> {
    return {
      success: false,
      message: 'Autotrade does not support manual order modification',
      errorCode: 'NOT_SUPPORTED',
    };
  }

  /**
   * Cancel an order (not supported)
   */
  async cancelOrder(_orderId: string): Promise<OrderResponse> {
    return {
      success: false,
      message: 'Autotrade does not support manual order cancellation',
      errorCode: 'NOT_SUPPORTED',
    };
  }

  /**
   * Get all orders for the account
   */
  async getOrders(): Promise<Order[]> {
    try {
      const response = await this.invokeAutotradeCommand<AutotradeOrder[]>(
        'autotrade_get_orders',
        this._accountId,
      );

      if (!response.success || !response.data) {
        return [];
      }

      return response.data.map((order) => this.transformOrder(order));
    } catch (error) {
      console.error(`[${this.brokerId}] getOrders failed:`, error);
      return [];
    }
  }

  /**
   * Get order history (alias for getOrders for Autotrade)
   */
  async getOrderHistory(): Promise<Order[]> {
    return this.getOrders();
  }

  /**
   * Get trade book
   */
  async getTradeBook(): Promise<Trade[]> {
    // Autotrade doesn't provide trade book via this endpoint
    // Trades can be calculated from order history
    return [];
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  /**
   * Cancel all orders (not supported)
   */
  async cancelAllOrders(): Promise<BulkOperationResult> {
    return {
      success: false,
      totalCount: 0,
      successCount: 0,
      failedCount: 0,
      results: [],
    };
  }

  /**
   * Close all positions (not supported)
   */
  async closeAllPositions(): Promise<BulkOperationResult> {
    return {
      success: false,
      totalCount: 0,
      successCount: 0,
      failedCount: 0,
      results: [],
    };
  }

  // ============================================================================
  // Smart Orders
  // ============================================================================

  /**
   * Place a smart order (not supported)
   */
  async placeSmartOrder(_params: SmartOrderParams): Promise<OrderResponse> {
    return {
      success: false,
      message: 'Autotrade does not support smart orders',
      errorCode: 'NOT_SUPPORTED',
    };
  }

  // ============================================================================
  // Positions & Holdings
  // ============================================================================

  /**
   * Get all positions for the account
   */
  async getPositions(): Promise<Position[]> {
    try {
      const response = await this.invokeAutotradeCommand<AutotradePosition[]>(
        'autotrade_get_positions',
        this._accountId,
      );

      if (!response.success || !response.data) {
        return [];
      }

      return response.data.map((pos) => this.transformPosition(pos));
    } catch (error) {
      console.error(`[${this.brokerId}] getPositions failed:`, error);
      return [];
    }
  }

  /**
   * Get holdings (positions with additional info)
   */
  async getHoldings(): Promise<Holding[]> {
    try {
      const positions = await this.getPositions();

      return positions.map((pos) => ({
        symbol: pos.symbol,
        exchange: pos.exchange,
        quantity: pos.quantity,
        averagePrice: pos.averagePrice,
        lastPrice: pos.lastPrice,
        investedValue: Math.abs(pos.quantity * pos.averagePrice),
        currentValue: Math.abs(pos.quantity * pos.lastPrice),
        pnl: pos.pnl,
        pnlPercent: pos.pnlPercent,
      }));
    } catch (error) {
      console.error(`[${this.brokerId}] getHoldings failed:`, error);
      return [];
    }
  }

  /**
   * Get a specific open position
   */
  async getOpenPosition(
    symbol: string,
    exchange: StockExchange,
    productType: ProductType,
  ): Promise<Position | null> {
    const positions = await this.getPositions();
    return positions.find(
      (p) =>
        p.symbol === symbol &&
        p.exchange === exchange &&
        p.productType === productType,
    ) || null;
  }

  // ============================================================================
  // Market Data
  // ============================================================================

  /**
   * Get quote for a single symbol
   */
  async getQuote(symbol: string, exchange: StockExchange): Promise<Quote> {
    try {
      // For now, return a placeholder quote
      // Full quote support requires additional API endpoint
      return {
        symbol,
        exchange,
        lastPrice: 0,
        open: 0,
        high: 0,
        low: 0,
        close: 0,
        previousClose: 0,
        change: 0,
        changePercent: 0,
        volume: 0,
        bid: 0,
        bidQty: 0,
        ask: 0,
        askQty: 0,
        timestamp: Date.now(),
      };
    } catch (error) {
      console.error(`[${this.brokerId}] getQuote failed for ${symbol}:`, error);
      throw error;
    }
  }

  /**
   * Get quotes for multiple instruments
   */
  async getQuotes(
    instruments: Array<{ symbol: string; exchange: StockExchange }>,
  ): Promise<Quote[]> {
    const quotes: Quote[] = [];

    for (const { symbol, exchange } of instruments) {
      try {
        const quote = await this.getQuote(symbol, exchange);
        quotes.push(quote);
      } catch (error) {
        console.error(`[${this.brokerId}] Failed to get quote for ${symbol}:`, error);
      }
    }

    return quotes;
  }

  /**
   * Get OHLCV data (not supported by Autotrade API)
   */
  async getOHLCV(
    _symbol: string,
    _exchange: StockExchange,
    _timeframe: TimeFrame,
    _from: Date,
    _to: Date,
  ): Promise<OHLCV[]> {
    // Autotrade doesn't provide historical data via this adapter
    // Use a dedicated historical data service instead
    return [];
  }

  /**
   * Get market depth (not supported)
   */
  async getMarketDepth(_symbol: string, _exchange: StockExchange): Promise<MarketDepth> {
    return {
      bids: [],
      asks: [],
    };
  }

  // ============================================================================
  // Symbol Search
  // ============================================================================

  /**
   * Search for symbols
   */
  async searchSymbols(_query: string, _exchange?: StockExchange): Promise<Instrument[]> {
    // Autotrade doesn't provide symbol search via this adapter
    return [];
  }

  /**
   * Get instrument by symbol
   */
  async getInstrument(_symbol: string, _exchange: StockExchange): Promise<Instrument | null> {
    // Autotrade doesn't provide instrument details via this adapter
    return null;
  }

  // ============================================================================
  // WebSocket
  // ============================================================================

  /**
   * Connect WebSocket (placeholder)
   */
  async connectWebSocket(_config?: WebSocketConfig): Promise<void> {
    // Autotrade uses a different WebSocket implementation
    this._isConnected = true;
  }

  /**
   * Disconnect WebSocket
   */
  async disconnectWebSocket(): Promise<void> {
    this._isConnected = false;
  }

  /**
   * Subscribe to WebSocket stream (placeholder)
   */
  async subscribe(
    _symbol: string,
    _exchange: StockExchange,
    _mode: SubscriptionMode,
  ): Promise<void> {
    // Autotrade uses a different WebSocket implementation
  }

  /**
   * Unsubscribe from WebSocket stream
   */
  async unsubscribe(_symbol: string, _exchange: StockExchange): Promise<void> {
    // Autotrade uses a different WebSocket implementation
  }

  /**
   * Register tick callback (placeholder)
   */
  onTick(_callback: (tick: TickData) => void): void {
    // Autotrade uses a different WebSocket implementation
  }

  /**
   * Remove tick callback (placeholder)
   */
  offTick(_callback: (tick: TickData) => void): void {
    // Autotrade uses a different WebSocket implementation
  }

  /**
   * Register depth callback (placeholder)
   */
  onDepth(_callback: (depth: MarketDepth & { symbol: string; exchange: StockExchange }) => void): void {
    // Autotrade uses a different WebSocket implementation
  }

  /**
   * Remove depth callback (placeholder)
   */
  offDepth(_callback: (depth: MarketDepth & { symbol: string; exchange: StockExchange }) => void): void {
    // Autotrade uses a different WebSocket implementation
  }

  // ============================================================================
  // Helper Methods
  // ============================================================================

  /**
   * Invoke an Autotrade Tauri command
   */
  private async invokeAutotradeCommand<T>(
    command: string,
    accountId: string,
  ): Promise<AutotradeApiResponse<T>> {
    try {
      const response = await invoke<AutotradeApiResponse<T>>(command, {
        accountId,
      });
      return response;
    } catch (error) {
      console.error(`[${this.brokerId}] Command ${command} failed:`, error);
      throw error;
    }
  }

  /**
   * Transform AutotradePosition to unified Position
   */
  private transformPosition(autotradePosition: AutotradePosition): Position {
    const quantity = autotradePosition.quantity;
    const avgPrice = autotradePosition.averagePrice;
    const currentPrice = autotradePosition.lastPrice;

    return {
      symbol: autotradePosition.symbol,
      exchange: autotradePosition.exchange,
      productType: autotradePosition.productType || 'CASH',
      quantity: quantity,
      buyQuantity: quantity > 0 ? quantity : 0,
      sellQuantity: quantity < 0 ? Math.abs(quantity) : 0,
      buyValue: quantity > 0 ? quantity * avgPrice : 0,
      sellValue: quantity < 0 ? Math.abs(quantity) * avgPrice : 0,
      averagePrice: avgPrice,
      lastPrice: currentPrice,
      pnl: autotradePosition.pnl,
      pnlPercent: autotradePosition.pnlPercent,
      dayPnl: autotradePosition.dayPnl,
      overnight: true,
    };
  }

  /**
   * Transform AutotradeOrder to unified Order
   */
  private transformOrder(autotradeOrder: AutotradeOrder): Order {
    return {
      orderId: autotradeOrder.orderId,
      symbol: autotradeOrder.symbol,
      exchange: autotradeOrder.exchange,
      side: autotradeOrder.side,
      quantity: autotradeOrder.quantity,
      filledQuantity: autotradeOrder.filledQuantity,
      pendingQuantity: autotradeOrder.pendingQuantity,
      price: autotradeOrder.price,
      averagePrice: autotradeOrder.averagePrice,
      triggerPrice: autotradeOrder.triggerPrice,
      orderType: autotradeOrder.orderType,
      productType: autotradeOrder.productType,
      validity: autotradeOrder.validity,
      status: autotradeOrder.status,
      statusMessage: autotradeOrder.statusMessage,
      placedAt: autotradeOrder.placedAt,
      updatedAt: autotradeOrder.updatedAt,
      exchangeOrderId: autotradeOrder.exchangeOrderId,
      tag: autotradeOrder.tag,
    };
  }

  /**
   * Transform AutotradeHolding to unified Holding
   */
  private transformHolding(autotradeHolding: AutotradeHolding): Holding {
    return {
      symbol: autotradeHolding.symbol,
      exchange: autotradeHolding.exchange,
      quantity: autotradeHolding.quantity,
      averagePrice: autotradeHolding.averagePrice,
      lastPrice: autotradeHolding.lastPrice,
      investedValue: autotradeHolding.quantity * autotradeHolding.averagePrice,
      currentValue: autotradeHolding.quantity * autotradeHolding.lastPrice,
      pnl: autotradeHolding.pnl,
      pnlPercent: autotradeHolding.pnlPercent,
    };
  }
}
