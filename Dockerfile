# Use a Python slim base image (Debian-based, common for Railway)
FROM python:3.12-slim-buster

# Install libGL.so.1 dependency for OpenCV
# Update apt lists, install the package, and clean up apt cache to keep image small
RUN apt-get update && \
    apt-get install -y libgl1-mesa-glx && \
    rm -rf /var/lib/apt/lists/*

# Set the working directory inside the container
WORKDIR /app

# Copy your requirements.txt and install Python dependencies
# It's good practice to copy requirements.txt and install dependencies first
# to leverage Docker's build cache
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of your application code
COPY . .

# Expose the port your Flask app listens on (if applicable)
# Default for Flask is 5000, but adjust if your app listens on another port
EXPOSE 5000

# Command to run your application when the container starts
# Replace 'app.py' with your actual main application file name
# If you use Gunicorn or Waitress for production, the command will be different, e.g.:
# CMD ["gunicorn", "-w", "4", "-b", "0.0.0.0:5000", "app:app"] # For a Flask app named 'app' in app.py
CMD ["python", "app.py"]