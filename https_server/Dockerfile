# Use Python 3.11 slim image as base
FROM python:3.11-slim

# Set working directory
WORKDIR /app

# Install required system dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    openssl \
    certbot \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy requirements first to leverage Docker cache
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy the server code and configuration
COPY secure_https_server.py .
COPY config.json .

# Create directories for volumes
RUN mkdir -p /app/firmware /app/logs /app/certs

# Expose the HTTPS port
EXPOSE 443

# Command to run the server
CMD ["python", "secure_https_server.py"]
