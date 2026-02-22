/**
 * Autotrade Positions View
 *
 * Displays Autotrade positions within the Portfolio tab
 * Shows positions, P&L, and performance for the automated trading portfolio
 */

import React, { useEffect, useState } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { formatCurrency, formatPercent } from '../portfolio/utils';

interface AutoPositionsViewProps {
  portfolioSummary: any;
  autotradePositions: any[];
  autotradePortfolioSummary?: any;
  currency: string;
  onRefresh?: () => Promise<void>;
  loading?: boolean;
}

const AutoPositionsView: React.FC<AutoPositionsViewProps> = ({
  portfolioSummary,
  autotradePositions,
  autotradePortfolioSummary,
  currency,
  onRefresh,
  loading,
}) => {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [lastRefresh, setLastRefresh] = useState<Date>(new Date());

  // Auto-refresh every 30 seconds
  useEffect(() => {
    const interval = setInterval(() => {
      setLastRefresh(new Date());
    }, 30000);
    return () => clearInterval(interval);
  }, []);

  // Get cash balance from portfolio summary if available
  const cashBalance = autotradePortfolioSummary?.availableCash || 0;
  const totalBalance = autotradePortfolioSummary?.totalBalance || 0;

  // Calculate totals from positions
  const totalMarketValue = autotradePositions.reduce((sum, p) => {
    return sum + Math.abs(p.quantity * p.lastPrice);
  }, 0);

  const totalCostBasis = autotradePositions.reduce((sum, p) => {
    return sum + Math.abs(p.quantity * p.averagePrice);
  }, 0);

  const totalPnl = autotradePositions.reduce((sum, p) => {
    return sum + (p.pnl || 0);
  }, 0);

  const totalDayChange = autotradePositions.reduce((sum, p) => {
    return sum + (p.dayPnl || 0);
  }, 0);

  // Sort by weight (largest positions first)
  const sortedPositions = [...autotradePositions].sort((a, b) => {
    const weightA = Math.abs(a.quantity * a.lastPrice);
    const weightB = Math.abs(b.quantity * b.lastPrice);
    return weightB - weightA;
  });

  return (
    <div style={{ height: '100%', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
      {/* Header / Stats */}
      <div style={{ display: 'flex', gap: '12px', marginBottom: '12px', flexWrap: 'wrap' }}>
        {/* Total Value */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.primary}`,
          borderRadius: '2px',
          padding: '12px',
          flex: '1',
          minWidth: '180px',
          display: 'flex',
          flexDirection: 'column',
          gap: '4px',
        }}>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>Total Market Value</div>
          <div style={{
            color: colors.primary,
            fontSize: fontSize.subheading,
            fontWeight: 700,
            fontFamily,
          }}>
            {formatCurrency(totalMarketValue, currency)}
          </div>
        </div>

        {/* Unrealized P&L */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${totalPnl >= 0 ? colors.success : colors.alert}`,
          borderRadius: '2px',
          padding: '12px',
          flex: '1',
          minWidth: '180px',
          display: 'flex',
          flexDirection: 'column',
          gap: '4px',
        }}>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>Unrealized P&L</div>
          <div style={{
            color: totalPnl >= 0 ? colors.success : colors.alert,
            fontSize: fontSize.subheading,
            fontWeight: 700,
            fontFamily,
          }}>
            {totalPnl >= 0 ? '+' : ''}{formatCurrency(totalPnl, currency)}
          </div>
          <div style={{
            color: totalPnl >= 0 ? colors.success : colors.alert,
            fontSize: fontSize.small,
          }}>
            {totalCostBasis > 0 ? formatPercent((totalPnl / totalCostBasis) * 100) : '0.00%'}
          </div>
        </div>

        {/* Day Change */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${totalDayChange >= 0 ? colors.success : colors.alert}`,
          borderRadius: '2px',
          padding: '12px',
          flex: '1',
          minWidth: '180px',
          display: 'flex',
          flexDirection: 'column',
          gap: '4px',
        }}>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>Day Change</div>
          <div style={{
            color: totalDayChange >= 0 ? colors.success : colors.alert,
            fontSize: fontSize.subheading,
            fontWeight: 700,
            fontFamily,
          }}>
            {totalDayChange >= 0 ? '+' : ''}{formatCurrency(totalDayChange, currency)}
          </div>
          <div style={{
            color: totalDayChange >= 0 ? colors.success : colors.alert,
            fontSize: fontSize.small,
          }}>
            {totalCostBasis > 0 ? formatPercent((totalDayChange / totalCostBasis) * 100) : '0.00%'}
          </div>
        </div>

        {/* Position Count */}
        <div style={{
          backgroundColor: colors.panel,
          border: '1px solid var(--ft-color-border, #2A2A2A)',
          borderRadius: '2px',
          padding: '12px',
          flex: '1',
          minWidth: '180px',
          display: 'flex',
          flexDirection: 'column',
          gap: '4px',
        }}>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>Positions</div>
          <div style={{
            color: 'var(--ft-color-accent, #00E5FF)',
            fontSize: fontSize.subheading,
            fontWeight: 700,
            fontFamily,
          }}>
            {autotradePositions.length}
          </div>
        </div>

        {/* Refresh Button */}
        <button
          onClick={onRefresh}
          disabled={loading}
          style={{
            backgroundColor: colors.primary,
            color: colors.background,
            border: 'none',
            borderRadius: '2px',
            padding: '8px 12px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            cursor: loading ? 'not-allowed' : 'pointer',
            fontFamily,
            textTransform: 'uppercase',
            flexShrink: 0,
          }}
        >
          <svg
            width={12}
            height={12}
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth={2.5}
            strokeLinecap="round"
            strokeLinejoin="round"
            style={{
              animation: loading ? 'spin 1s linear infinite' : 'none',
            }}
          >
            <path d="M21 12a9 9 0 0 0-9-9 9.75 9.75 0 0 0-6.74 2.74L3 8" />
            <path d="M3 3v5h5" />
            <path d="M3 12a9 9 0 0 0 9 9 9.75 9.75 0 0 0 6.74-2.74L21 16" />
            <path d="M16 21h5v-5" />
          </svg>
          {loading ? 'Refreshing...' : 'Refresh'}
        </button>
      </div>

      {/* Account Summary Section */}
      {autotradePortfolioSummary && (
        <div style={{
          backgroundColor: colors.panel,
          border: '1px solid var(--ft-color-border, #2A2A2A)',
          borderRadius: '2px',
          padding: '12px',
          marginBottom: '12px',
        }}>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '8px' }}>Account Summary</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px' }}>
            <div>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>Total Balance</div>
              <div style={{ color: colors.primary, fontSize: fontSize.small, fontWeight: 600 }}>
                {formatCurrency(totalBalance, currency)}
              </div>
            </div>
            <div>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>Available Cash</div>
              <div style={{ color: colors.text, fontSize: fontSize.small, fontWeight: 600 }}>
                {formatCurrency(cashBalance, currency)}
              </div>
            </div>
            <div>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>Total Market Value</div>
              <div style={{ color: colors.text, fontSize: fontSize.small, fontWeight: 600 }}>
                {formatCurrency(totalMarketValue, currency)}
              </div>
            </div>
            <div>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>Unrealized P&L</div>
              <div style={{
                color: totalPnl >= 0 ? colors.success : colors.alert,
                fontSize: fontSize.small,
                fontWeight: 600,
              }}>
                {totalPnl >= 0 ? '+' : ''}{formatCurrency(totalPnl, currency)}
                {totalCostBasis > 0 && (
                  <span style={{ color: colors.textMuted, marginLeft: '4px', fontSize: fontSize.tiny }}>
                    ({formatPercent((totalPnl / totalCostBasis) * 100)})
                  </span>
                )}
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Positions Table */}
      <div style={{
        flex: 1,
        backgroundColor: colors.panel,
        border: '1px solid var(--ft-color-border, #2A2A2A)',
        borderRadius: '2px',
        overflow: 'hidden',
        display: 'flex',
        flexDirection: 'column',
      }}>
        {/* Table Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1.5fr 1fr 0.8fr 0.8fr 0.8fr 0.8fr 0.8fr 0.8fr',
          gap: '8px',
          padding: '10px 12px',
          backgroundColor: 'var(--ft-color-header, #1A1A1A)',
          borderBottom: '2px solid var(--ft-color-border, #2A2A2A)',
          fontSize: fontSize.tiny,
          fontWeight: 700,
          fontFamily,
          textTransform: 'uppercase',
          color: colors.textMuted,
          letterSpacing: '0.5px',
        }}>
          <div>Symbol</div>
          <div>Exchange</div>
          <div>Quantity</div>
          <div>Avg Price</div>
          <div>Last Price</div>
          <div>Market Value</div>
          <div>P&L</div>
          <div>P&L %</div>
        </div>

        {/* Table Body */}
        <div style={{ flex: 1, overflow: 'auto' }}>
          {autotradePositions.length === 0 ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              color: '#4A4A4A',
              padding: '48px',
              textAlign: 'center',
            }}>
              <div style={{ color: colors.textMuted, fontSize: fontSize.small, marginBottom: '8px' }}>
                No Autotrade positions found
              </div>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
                Positions will appear here when your Autotrade strategy has active trades
              </div>
            </div>
          ) : (
            sortedPositions.map((position, index) => {
              const quantity = position.quantity;
              const avgPrice = position.averagePrice;
              const lastPrice = position.lastPrice;
              const pnl = position.pnl || 0;
              const pnlPercent = position.pnlPercent || 0;
              const marketValue = Math.abs(quantity * lastPrice);

              return (
                <div
                  key={`${position.symbol}-${position.exchange}-${index}`}
                  style={{
                    display: 'grid',
                    gridTemplateColumns: '1.5fr 1fr 0.8fr 0.8fr 0.8fr 0.8fr 0.8fr 0.8fr',
                    gap: '8px',
                    padding: '8px 12px',
                    borderBottom: index < autotradePositions.length - 1
                      ? `1px solid var(--ft-color-border, #2A2A2A)`
                      : 'none',
                    backgroundColor: index % 2 === 0 ? 'transparent' : `${colors.primary}05`,
                    fontSize: fontSize.small,
                    fontFamily,
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = 'var(--ft-color-hover, #1F1F1F)';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = index % 2 === 0 ? 'transparent' : `${colors.primary}05`;
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', fontWeight: 600, color: 'var(--ft-color-accent, #00E5FF)' }}>
                    {position.symbol}
                  </div>
                  <div style={{ color: colors.textMuted }}>
                    {position.exchange}
                  </div>
                  <div style={{ color: colors.text }}>
                    {Math.abs(quantity).toFixed(0)}
                  </div>
                  <div style={{ color: colors.textMuted, fontFamily: 'monospace' }}>
                    {avgPrice.toFixed(2)}
                  </div>
                  <div style={{ color: colors.text, fontFamily: 'monospace' }}>
                    {lastPrice.toFixed(2)}
                  </div>
                  <div style={{ color: colors.text, fontFamily: 'monospace' }}>
                    {formatCurrency(marketValue, currency)}
                  </div>
                  <div style={{
                    color: pnl >= 0 ? colors.success : colors.alert,
                    fontFamily: 'monospace',
                    fontWeight: 600,
                  }}>
                    {pnl >= 0 ? '+' : ''}{formatCurrency(pnl, currency)}
                  </div>
                  <div style={{
                    color: pnlPercent >= 0 ? colors.success : colors.alert,
                    fontFamily: 'monospace',
                  }}>
                    {formatPercent(pnlPercent)}
                  </div>
                </div>
              );
            })
          )}
        </div>
      </div>
    </div>
  );
};

export default AutoPositionsView;
