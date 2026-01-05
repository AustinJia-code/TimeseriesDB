// Configuration
const MAX_DATA_POINTS = 100;
const COLORS = [
    'rgba(59, 130, 246, 1)',   // Blue
    'rgba(16, 185, 129, 1)',   // Green
    'rgba(239, 68, 68, 1)',    // Red
    'rgba(245, 158, 11, 1)',   // Orange
    'rgba(139, 92, 246, 1)',   // Purple
    'rgba(236, 72, 153, 1)',   // Pink
    'rgba(34, 197, 94, 1)',    // Emerald
    'rgba(251, 191, 36, 1)',   // Yellow
];

// State
let charts = {};
let selectedTags = new Set();
let availableTags = [];
let pollingInterval = null;
let isPolling = false;
let tagColorMap = {};

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    setupEventListeners();
    loadAvailableTags();
    updateStatus('disconnected', 'Ready');
});

// Setup event listeners
function setupEventListeners() {
    document.getElementById('start-stop-btn').addEventListener('click', togglePolling);
    document.getElementById('test-connection-btn').addEventListener('click', testConnection);
    document.getElementById('refresh-tags-btn').addEventListener('click', loadAvailableTags);
    
    document.getElementById('poll-interval').addEventListener('change', () => {
        if (isPolling) {
            stopPolling();
            startPolling();
        }
    });
    
    document.getElementById('api-url').addEventListener('change', () => {
        if (isPolling) {
            stopPolling();
            startPolling();
        }
        loadAvailableTags();
    });
}

// Fetch available tags from API
async function loadAvailableTags() {
    const apiUrl = document.getElementById('api-url').value.trim();
    
    if (!apiUrl) {
        showError('Please enter an API URL');
        return;
    }

    const container = document.getElementById('tags-container');
    container.innerHTML = '<div class="loading-tags">Loading tags...</div>';

    try {
        const url = `${apiUrl}/tags`;
        const response = await fetch(url, {
            method: 'GET',
            headers: { 'Accept': 'application/json' },
            mode: 'cors'
        });

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const tags = await response.json();
        availableTags = tags;
        renderTags(tags);
        
        // Assign colors to tags
        tags.forEach((tag, index) => {
            if (!tagColorMap[tag]) {
                tagColorMap[tag] = COLORS[index % COLORS.length];
            }
        });

    } catch (error) {
        console.error('Error loading tags:', error);
        container.innerHTML = `<div class="loading-tags" style="color: var(--danger);">Error: ${error.message}</div>`;
        showError(`Failed to load tags: ${error.message}`);
    }
}

// Render tags with checkboxes
function renderTags(tags) {
    const container = document.getElementById('tags-container');
    
    if (tags.length === 0) {
        container.innerHTML = '<div class="loading-tags">No tags available</div>';
        return;
    }

    container.innerHTML = tags.map(tag => `
        <div class="tag-item ${selectedTags.has(tag) ? 'selected' : ''}" data-tag="${tag}">
            <div class="tag-checkbox ${selectedTags.has(tag) ? 'checked' : ''}"></div>
            <span class="tag-label">${tag}</span>
        </div>
    `).join('');

    // Add click handlers
    container.querySelectorAll('.tag-item').forEach(item => {
        item.addEventListener('click', () => toggleTag(item.dataset.tag));
    });
}

// Toggle tag selection
function toggleTag(tag) {
    if (selectedTags.has(tag)) {
        selectedTags.delete(tag);
        removeChart(tag);
    } else {
        selectedTags.add(tag);
        createChart(tag);
    }
    
    renderTags(availableTags);
    updateSelectedCount();
}

// Create a new chart for a tag
function createChart(tag) {
    if (charts[tag]) return; // Chart already exists

    const chartsContainer = document.getElementById('charts-container');
    
    // Remove empty state if present
    const emptyState = chartsContainer.querySelector('.empty-state');
    if (emptyState) {
        emptyState.remove();
    }

    // Create card
    const card = document.createElement('div');
    card.className = 'sensor-card';
    card.id = `card-${tag}`;
    card.innerHTML = `
        <div class="sensor-header">
            <div class="sensor-title">${tag}</div>
            <div class="sensor-value" id="value-${tag}">--</div>
        </div>
        <div class="chart-container">
            <canvas id="chart-${tag}"></canvas>
        </div>
    `;
    chartsContainer.appendChild(card);

    // Create Chart.js instance
    const ctx = document.getElementById(`chart-${tag}`).getContext('2d');
    const color = tagColorMap[tag] || COLORS[0];
    
    charts[tag] = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: tag,
                data: [],
                borderColor: color,
                backgroundColor: color.replace('1)', '0.1)'),
                borderWidth: 2,
                fill: true,
                tension: 0.4,
                pointRadius: 0,
                pointHoverRadius: 4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false },
                    tooltip: {
                        mode: 'index',
                        intersect: false,
                        callbacks: {
                            label: function(context) {
                                return `Value: ${context.parsed.y.toFixed(2)}`;
                            },
                            title: function(context) {
                                const label = context[0].label;
                                if (!label) return '';
                                
                                const timestamp = parseInt(label);
                                if (isNaN(timestamp) || timestamp <= 0) {
                                    return `Time: ${label}`;
                                }
                                
                                const date = new Date(timestamp);
                                if (isNaN(date.getTime())) {
                                    return `Time: ${label}`;
                                }
                                
                                return date.toLocaleString();
                            }
                        }
                    }
            },
            scales: {
                x: {
                    display: true,
                    title: { display: true, text: 'Time', color: '#94a3b8' },
                    ticks: {
                        maxTicksLimit: 8,
                        color: '#94a3b8',
                        callback: function(value, index) {
                            const label = this.getLabelForValue(value);
                            if (!label) return '';
                            
                            const timestamp = parseInt(label);
                            if (isNaN(timestamp) || timestamp <= 0) {
                                return label;
                            }
                            
                            const date = new Date(timestamp);
                            if (isNaN(date.getTime())) {
                                return label;
                            }
                            
                            return date.toLocaleTimeString();
                        }
                    },
                    grid: { color: 'rgba(71, 85, 105, 0.3)' }
                },
                y: {
                    display: true,
                    title: { display: true, text: 'Value', color: '#94a3b8' },
                    ticks: { color: '#94a3b8' },
                    grid: { color: 'rgba(71, 85, 105, 0.3)' },
                    beginAtZero: false
                }
            },
            interaction: {
                mode: 'nearest',
                axis: 'x',
                intersect: false
            }
        }
    });
}

// Remove chart for a tag
function removeChart(tag) {
    if (charts[tag]) {
        charts[tag].destroy();
        delete charts[tag];
    }
    
    const card = document.getElementById(`card-${tag}`);
    if (card) {
        card.remove();
    }

    // Show empty state if no charts
    if (selectedTags.size === 0) {
        const chartsContainer = document.getElementById('charts-container');
        chartsContainer.innerHTML = `
            <div class="empty-state">
                <div class="empty-icon">📊</div>
                <h3>No Tags Selected</h3>
                <p>Select tags from the sidebar to view their data</p>
            </div>
        `;
    }
}

// Update chart with new data
function updateChart(tag, data) {
    if (!data || data.length === 0 || !charts[tag]) return;

    const chart = charts[tag];
    
    // Filter out invalid data points
    data = data.filter(d => {
        // Check if timestamp is valid (positive number)
        if (typeof d.ts !== 'number' || isNaN(d.ts) || d.ts <= 0) {
            console.warn(`Invalid timestamp for ${tag}:`, d.ts);
            return false;
        }
        // Check if value is valid
        if (typeof d.val !== 'number' || isNaN(d.val)) {
            console.warn(`Invalid value for ${tag}:`, d.val);
            return false;
        }
        return true;
    });
    
    if (data.length === 0) {
        console.warn(`No valid data points for ${tag}`);
        return;
    }
    
    // Sort data by timestamp
    data.sort((a, b) => a.ts - b.ts);

    // Extract timestamps and values (ensure timestamps are valid)
    const timestamps = data.map(d => {
        // Handle both number and string timestamps
        const ts = typeof d.ts === 'number' ? d.ts : parseInt(d.ts);
        if (isNaN(ts) || ts <= 0) {
            console.warn(`Invalid timestamp in mapping for ${tag}:`, d.ts, 'Full data:', d);
            return Date.now().toString(); // Fallback to current time
        }
        // Check if timestamp is reasonable (not a value that got swapped)
        if (ts < 1000000000) { // Less than year 2001 in ms
            console.warn(`Suspiciously small timestamp for ${tag}:`, ts, 'Full data:', d);
        }
        if (ts > 9999999999999) { // More than year 2286
            console.warn(`Suspiciously large timestamp for ${tag}:`, ts, 'Full data:', d);
        }
        return ts.toString();
    });
    
    const values = data.map(d => {
        const val = typeof d.val === 'number' ? d.val : parseFloat(d.val);
        // Check if value looks like a timestamp (very large number)
        if (val > 1000000000000) {
            console.warn(`Value looks like timestamp for ${tag}:`, val, 'Full data:', d);
        }
        return val;
    });

    // Keep only the last MAX_DATA_POINTS
    if (timestamps.length > MAX_DATA_POINTS) {
        chart.data.labels = timestamps.slice(-MAX_DATA_POINTS);
        chart.data.datasets[0].data = values.slice(-MAX_DATA_POINTS);
    } else {
        chart.data.labels = timestamps;
        chart.data.datasets[0].data = values;
    }

    // Update the chart
    chart.update('none');

    // Update the latest value display
    if (values.length > 0) {
        const latestValue = values[values.length - 1];
        const valueElement = document.getElementById(`value-${tag}`);
        if (valueElement) {
            valueElement.textContent = latestValue.toFixed(2);
        }
    }
}

// Fetch data from API
async function fetchSensorData(tag, apiUrl) {
    try {
        const url = `${apiUrl}/read?tag=${tag}`;
        const response = await fetch(url, {
            method: 'GET',
            headers: { 'Accept': 'application/json' },
            mode: 'cors'
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const text = await response.text();
        let data;
        try {
            data = JSON.parse(text);
        } catch (parseError) {
            console.error(`JSON parse error for ${tag}:`, parseError);
            console.error('Response text:', text.substring(0, 500));
            throw new Error(`Invalid JSON response: ${parseError.message}`);
        }
        
        // Validate data structure
        if (!Array.isArray(data)) {
            console.error(`Invalid data format for ${tag}:`, data);
            return null;
        }
        
        // Log sample data for debugging
        if (data.length > 0) {
            console.log(`Sample data for ${tag}:`, data[0]);
            // Check if fields are swapped
            const first = data[0];
            if (first.ts && typeof first.ts === 'number' && first.ts > 1000000000000) {
                console.warn(`Large timestamp detected for ${tag}:`, first.ts);
            }
            if (first.val && typeof first.val === 'number' && first.val > 1000000000000) {
                console.warn(`Large value detected for ${tag} (might be timestamp):`, first.val);
            }
        }
        
        return data;
    } catch (error) {
        console.error(`Error fetching data for ${tag}:`, error.message);
        return null;
    }
}

// Poll all selected sensors
async function pollSensors() {
    const apiUrl = document.getElementById('api-url').value.trim();
    
    if (!apiUrl) {
        updateStatus('disconnected', 'API URL required');
        showError('Please enter an API URL');
        return;
    }

    if (selectedTags.size === 0) {
        updateStatus('disconnected', 'No tags selected');
        return;
    }

    updateStatus('connecting', 'Polling...');

    const promises = Array.from(selectedTags).map(tag => 
        fetchSensorData(tag, apiUrl).then(data => ({ tag, data }))
    );

    const results = await Promise.allSettled(promises);
    
    let hasData = false;
    let hasError = false;

    results.forEach((result) => {
        if (result.status === 'fulfilled') {
            const { tag, data } = result.value;
            if (data !== null && Array.isArray(data)) {
                updateChart(tag, data);
                hasData = true;
            } else {
                hasError = true;
            }
        } else {
            hasError = true;
        }
    });

    if (hasData && !hasError) {
        updateStatus('connected', 'Connected');
        hideError();
    } else if (hasError) {
        updateStatus('disconnected', 'Connection Error');
    }
}

// Update status indicator
function updateStatus(status, text) {
    const badge = document.getElementById('status-badge');
    const dot = badge.querySelector('.status-dot');
    const textEl = document.getElementById('status-text');
    
    dot.className = `status-dot ${status}`;
    textEl.textContent = text;
}

// Update selected count
function updateSelectedCount() {
    const count = selectedTags.size;
    const countEl = document.getElementById('selected-count');
    countEl.textContent = `${count} tag${count !== 1 ? 's' : ''} selected`;
}

// Show error message
function showError(message) {
    const errorDiv = document.getElementById('error-message');
    if (errorDiv && message) {
        errorDiv.textContent = message;
        errorDiv.style.display = 'block';
        setTimeout(() => {
            errorDiv.style.display = 'none';
        }, 10000);
    }
}

// Hide error message
function hideError() {
    const errorDiv = document.getElementById('error-message');
    if (errorDiv) {
        errorDiv.style.display = 'none';
    }
}

// Test connection
async function testConnection() {
    const apiUrl = document.getElementById('api-url').value.trim();
    
    if (!apiUrl) {
        showError('Please enter an API URL');
        return;
    }

    updateStatus('connecting', 'Testing...');

    try {
        const url = `${apiUrl}/tags`;
        const response = await fetch(url, {
            method: 'GET',
            headers: { 'Accept': 'application/json' },
            mode: 'cors'
        });

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const tags = await response.json();
        updateStatus('connected', 'Connection OK');
        showError('');
        loadAvailableTags();
        
    } catch (error) {
        console.error('Connection test failed:', error);
        updateStatus('disconnected', 'Connection Failed');
        showError(`Connection test failed: ${error.message}`);
    }
}

// Start polling
function startPolling() {
    if (isPolling) return;
    if (selectedTags.size === 0) {
        showError('Please select at least one tag to monitor');
        return;
    }

    isPolling = true;
    const interval = parseInt(document.getElementById('poll-interval').value) || 1000;
    
    pollSensors();
    pollingInterval = setInterval(pollSensors, interval);
    
    const btn = document.getElementById('start-stop-btn');
    btn.textContent = 'Stop Polling';
    btn.className = 'btn btn-danger';
}

// Stop polling
function stopPolling() {
    if (!isPolling) return;

    isPolling = false;
    if (pollingInterval) {
        clearInterval(pollingInterval);
        pollingInterval = null;
    }
    
    const btn = document.getElementById('start-stop-btn');
    btn.textContent = 'Start Polling';
    btn.className = 'btn btn-primary';
    
    updateStatus('disconnected', 'Stopped');
}

// Toggle polling
function togglePolling() {
    if (isPolling) {
        stopPolling();
    } else {
        startPolling();
    }
}
