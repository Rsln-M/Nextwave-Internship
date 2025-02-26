# Secure HTTPS File Server

A lightweight, secure Python HTTPS server for serving files with built-in security features.

## Features

- **HTTPS Support**: Secure file serving with SSL/TLS encryption
- **Rate Limiting**: Prevents abuse by limiting request frequency
- **DoS Protection**: Automatically detects and blocks potential DoS attacks
- **Certificate Management**: Automated certificate renewal checks
- **Directory Access Control**: Restricts access to specified directories only
- **Directory Listing**: Optional directory browsing for allowed directories
- **Configurable**: JSON-based configuration for easy customization

## Quick Start

1. Install dependencies:
   ```
   pip install limits
   ```

2. Configure your SSL certificates in `config.json` or use the defaults:
   ```json
   {
     "cert_path": "/etc/letsencrypt/live/yourdomain.com/fullchain.pem",
     "key_path": "/etc/letsencrypt/live/yourdomain.com/privkey.pem"
   }
   ```

3. Run the server:
   ```
   python server.py
   ```

## Configuration

Edit `config.json` to customize server behavior:

```json
{
  "server_port": 443,
  "rate_limit": "10 per minute",
  "dos_threshold": 20,
  "dos_interval": 60,
  "allowed_directories": ["firmware"],
  "index_files": ["index.html", "index.htm"]
}
```

## Security Features

- Directory traversal prevention
- Request rate limiting
- Automatic blocking of abusive IPs
- Secure file serving with proper MIME types

## Logs

Logs are stored in `server.log` with automatic rotation when the file size exceeds 10MB.
