# MQTT QoS Performance Analysis

## Background

MQTT, a lightweight messaging protocol, supports three Quality of Service (QoS) levels:

- **QoS 0**: At most once delivery.
- **QoS 1**: At least once delivery.
- **QoS 2**: Exactly once delivery.

Additionally, MQTT 5 introduced features like user properties, reason codes, and improved error handling, differentiating it from MQTT 3.

## Experimental Setup

We used the **EMQX broker** and the **MQTT-Paho library** to test the performance of different QoS levels.

## Observations

### 1. QoS Levels
- **QoS 0**: Fastest but unreliable in high packet loss scenarios.
- **QoS 1**: Guaranteed delivery with slight latency due to acknowledgments.
- **QoS 2**: Slowest due to its two-step acknowledgment process but ensures no duplicates.

### 2. MQTT 3 vs. MQTT 5
- **MQTT 5** provides detailed **reason codes** for errors, aiding debugging.
- Features like **message expiry** improve control over message lifetimes.

## Recommendations

For different applications:

- **QoS 0**: Use for non-critical telemetry.
- **QoS 1**: Suitable for general use cases requiring reliability.
- **QoS 2**: Ideal for critical transactions.

Further analysis with tools like **Wireshark** could provide deeper insights into packet-level differences.
