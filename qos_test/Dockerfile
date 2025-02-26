# Use an official Python runtime as the base image
FROM python:3.9-slim

# Set the working directory in the container
WORKDIR /app

# Copy the Python script and firmware files into the container
COPY test.py .
COPY firmware/ ./firmware/

# Install the required Python packages
RUN pip install paho-mqtt

# Expose the MQTT broker port (optional, for documentation purposes)
EXPOSE 1883

# Run the Python script
CMD ["python", "test.py"]
