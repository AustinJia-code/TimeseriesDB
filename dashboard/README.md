### Prerequisites

1. Make sure the `tsdb_server` is running on port 9090
2. Make sure the `load_gen` is running to generate sensor data

### Running the Dashboard

```bash
cd dashboard
python3 -m http.server 8000
```
Then open: `http://localhost:8000`

### Configuration

- **Poll Interval**: Adjust how often the dashboard polls the API (default: 1000ms)
- **API URL**: Change the API endpoint if your server is running on a different address/port (default: `http://localhost:9090`)

Each sensor card shows:
- Current/latest value
- Real-time line chart with the last 50 data points
- Sensor tag name