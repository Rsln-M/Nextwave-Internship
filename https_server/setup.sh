#!/bin/bash
set -e

# Create necessary directories
mkdir -p firmware logs certs

# Create a default config file if it doesn't exist
if [ ! -f config.json ]; then
    cat > config.json << 'EOF'
{
    "server_port": 443,
    "rate_limit": "10 per minute",
    "dos_threshold": 20,
    "dos_interval": 60,
    "cert_check_interval": 86400,
    "cert_path": "/app/certs/fullchain.pem",
    "key_path": "/app/certs/privkey.pem",
    "blocked_ips": [],
    "document_root": ".",
    "allowed_directories": ["firmware"],
    "index_files": ["index.html", "index.htm"]
}
EOF
    echo "Created default config.json"
fi

# Generate self-signed certificates for development
if [ ! -f certs/fullchain.pem ] || [ ! -f certs/privkey.pem ]; then
    echo "Generating self-signed certificates for development..."
    openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
            -keyout certs/privkey.pem \
            -out certs/fullchain.pem \
            -subj '/CN=localhost'
    echo "Self-signed certificates generated in ./certs directory"
fi

# Create a sample firmware file for testing
echo "Creating sample firmware file for testing..."
mkdir -p firmware
echo "This is sample firmware v1.0" > firmware/sample_firmware_v1.0.bin
echo "This is sample firmware v2.0" > firmware/sample_firmware_v2.0.bin

# Save the server code
cat > secure_https_server.py << 'EOF'
# Paste the entire server code here
# For brevity this is omitted, but would contain the entire secure_https_server.py content
EOF

echo "Setup complete! Run 'docker-compose up -d' to start the server."
