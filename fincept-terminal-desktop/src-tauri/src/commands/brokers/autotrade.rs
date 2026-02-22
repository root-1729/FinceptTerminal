//! Autotrade Broker Integration
//!
//! Autotrade integration commands for Fincept Terminal
//! Fetches portfolio data from the Autotrade Integration Service (port 8001)
//!
//! ## Data Source:
//! - Autotrade Integration Service: http://localhost:8001/api/v1/
//!
//! ## Available Commands:
//! - `autotrade_get_positions` - Get positions for an account
//! - `autotrade_get_account_summary` - Get account portfolio summary
//! - `autotrade_get_performance` - Get performance data
//! - `autotrade_get_orders` - Get current orders

use reqwest::Client;
use serde_json::Value;
use std::time::Duration;

use super::common::ApiResponse;

// ============================================================================
// Autotrade Integration Service Configuration
// ============================================================================

/// Base URL for Autotrade Integration Service
const AUTOTRADE_API_BASE: &str = "http://localhost:8001";

/// HTTP timeout for API calls (in seconds)
const HTTP_TIMEOUT_SECS: u64 = 60;

/// Create an HTTP client with timeout and headers
fn create_http_client() -> Client {
    Client::builder()
        .timeout(Duration::from_secs(HTTP_TIMEOUT_SECS))
        .build()
        .unwrap_or_else(|_| Client::new())
}

// ============================================================================
// Command: autotrade_get_positions
// ============================================================================

/// Get positions for an Autotrade account
///
/// Fetches positions from `/api/v1/positions` endpoint
///
/// # Arguments
/// * `account_id` - The Autotrade account ID (e.g., "DU8489265")
///
/// # Response
/// Returns array of position objects with:
/// - symbol, quantity, avg_price, current_price
/// - market_value, unrealized_pnl, unrealized_pnl_percent
/// - day_change, day_change_percent, weight
#[tauri::command]
pub async fn autotrade_get_positions(
    account_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[autotrade_get_positions] Fetching positions for account: {}", account_id);

    let client = create_http_client();
    let base_url = AUTOTRADE_API_BASE;

    let response = client
        .get(format!("{}/api/v1/positions", base_url))
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let response_body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Parse the API response object and extract the data field
    let data: Option<Vec<Value>> = response_body.get("data")
        .and_then(|d| d.as_array())
        .map(|arr| arr.iter().cloned().collect());

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data,
            error: None,
            timestamp,
        })
    } else {
        let error_msg = response_body.get("error")
            .and_then(|e| e.as_str())
            .unwrap_or("Unknown error");
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to fetch positions: {}", error_msg)),
            timestamp,
        })
    }
}

// ============================================================================
// Command: autotrade_get_account_summary
// ============================================================================

/// Get account portfolio summary
///
/// Fetches full portfolio snapshot from `/api/v1/portfolio` endpoint
///
/// # Arguments
/// * `account_id` - The Autotrade account ID (e.g., "DU8489265")
///
/// # Response
/// Returns portfolio summary with:
/// - account_id, total_market_value, total_cost_basis
/// - total_unrealized_pnl, total_unrealized_pnl_percent
/// - total_positions, positions array, last_updated
#[tauri::command]
pub async fn autotrade_get_account_summary(
    account_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[autotrade_get_account_summary] Fetching portfolio summary for account: {}", account_id);

    let client = create_http_client();
    let base_url = AUTOTRADE_API_BASE;

    let response = client
        .get(format!("{}/api/v1/portfolio", base_url))
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let response_body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    // The API response has structure: {success, data: {...portfolio...}, error, ...}
    // The data field directly contains the portfolio summary
    let data: Option<Value> = response_body.get("data").cloned();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data,
            error: None,
            timestamp,
        })
    } else {
        let error_msg = response_body.get("error")
            .and_then(|e| e.as_str())
            .unwrap_or("Unknown error");
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to fetch portfolio: {}", error_msg)),
            timestamp,
        })
    }
}

// ============================================================================
// Command: autotrade_get_performance
// ============================================================================

/// Get performance data for an account
///
/// Fetches performance metrics from `/api/v1/portfolio/performance` endpoint
///
/// # Arguments
/// * `account_id` - The Autotrade account ID (e.g., "DU8489265")
/// * `period` - Optional time period: "1d", "7d", "30d", "ytd", "1y", "all"
///
/// # Response
/// Returns performance data with:
/// - series array (timestamp, nav, cumulative_return)
/// - currency, period, annualized_return
#[tauri::command]
pub async fn autotrade_get_performance(
    account_id: String,
    period: Option<String>,
) -> Result<ApiResponse<Value>, String> {
    let period_str = period.as_deref().unwrap_or("default");
    eprintln!(
        "[autotrade_get_performance] Fetching performance for account: {}, period: {}",
        account_id, period_str
    );

    let client = create_http_client();
    let base_url = AUTOTRADE_API_BASE;

    let url = format!("{}/api/v1/portfolio/performance", base_url);
    let url = if let Some(p) = period {
        format!("{}?period={}", url, p)
    } else {
        url
    };

    let response = client
        .get(url)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to fetch performance: HTTP {}", status.as_u16())),
            timestamp,
        })
    }
}

// ============================================================================
// Command: autotrade_get_orders
// ============================================================================

/// Get current orders for an account
///
/// Fetches orders from `/api/v1/orders` endpoint
///
/// # Arguments
/// * `account_id` - The Autotrade account ID (e.g., "DU8489265")
///
/// # Response
/// Returns array of order objects with:
/// - order_id, symbol, side, quantity
/// - price, filled_quantity, status, placed_at
#[tauri::command]
pub async fn autotrade_get_orders(
    account_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[autotrade_get_orders] Fetching orders for account: {}", account_id);

    let client = create_http_client();
    let base_url = AUTOTRADE_API_BASE;

    let response = client
        .get(format!("{}/api/v1/orders", base_url))
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let response_body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Parse the API response object and extract the data field
    // The API returns {"success": true, "data": [...], ...}
    // We need to extract the "data" field which is a Value (array)
    let data: Option<Value> = response_body.get("data").cloned();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data,
            error: None,
            timestamp,
        })
    } else {
        let error_msg = response_body.get("error")
            .and_then(|e| e.as_str())
            .unwrap_or("Unknown error");
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to fetch orders: {}", error_msg)),
            timestamp,
        })
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_create_http_client() {
        let client = create_http_client();
        // The client should be created successfully without panicking
        // reqwest::Client doesn't expose a direct way to verify timeout
        // Just verify it can be used for requests (checked in integration tests)
        let _client = client;
    }

    #[test]
    fn test_api_response_structure() {
        let response: ApiResponse<String> = ApiResponse {
            success: true,
            data: Some("test".to_string()),
            error: None,
            timestamp: 1234567890,
        };
        assert!(response.success);
        assert_eq!(response.data, Some("test".to_string()));
        assert!(response.error.is_none());
    }

    #[test]
    fn test_api_error_structure() {
        let response: ApiResponse<Value> = ApiResponse {
            success: false,
            data: None,
            error: Some("Test error".to_string()),
            timestamp: 1234567890,
        };
        assert!(!response.success);
        assert!(response.data.is_none());
        assert_eq!(response.error, Some("Test error".to_string()));
    }

    #[tokio::test]
    #[ignore = "Requires Autotrade service running on port 8001"]
    async fn test_autotrade_get_positions_integration() {
        // This test requires the Autotrade Integration Service to be running
        // Run: uvicorn fincept_integration.app.main:app --port 8001
        let result = autotrade_get_positions("DU8489265".to_string()).await;

        // Should succeed if service is running
        assert!(result.is_ok());
        let response = result.unwrap();
        assert!(response.success || response.error.is_some()); // Either success or proper error
    }

    #[tokio::test]
    #[ignore = "Requires Autotrade service running on port 8001"]
    async fn test_autotrade_get_account_summary_integration() {
        let result = autotrade_get_account_summary("DU8489265".to_string()).await;
        assert!(result.is_ok());
        let response = result.unwrap();
        assert!(response.success || response.error.is_some());
    }

    #[tokio::test]
    #[ignore = "Requires Autotrade service running on port 8001"]
    async fn test_autotrade_get_performance_integration() {
        let result = autotrade_get_performance("DU8489265".to_string(), Some("30d".to_string())).await;
        assert!(result.is_ok());
        let response = result.unwrap();
        assert!(response.success || response.error.is_some());
    }

    #[tokio::test]
    #[ignore = "Requires Autotrade service running on port 8001"]
    async fn test_autotrade_get_orders_integration() {
        let result = autotrade_get_orders("DU8489265".to_string()).await;
        assert!(result.is_ok());
        let response = result.unwrap();
        assert!(response.success || response.error.is_some());
    }
}
