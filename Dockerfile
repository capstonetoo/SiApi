# Gunakan base image Python yang sesuai dengan Debian Bookworm (saat ini stable)
FROM python:3.10-slim-bookworm # <--- UBAH DI SINI!

# Instal dependensi sistem yang diperlukan oleh OpenCV
RUN apt-get update && \
    apt-get install -y libgl1-mesa-glx libxrender1 libsm6 libxext6 && \
    rm -rf /var/lib/apt/lists/*

# Atur working directory di dalam container
WORKDIR /app

# Copy requirements.txt dan install dependensi Python
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy semua kode aplikasi Anda ke dalam container
COPY . .

# Exposure port yang akan digunakan Uvicorn
EXPOSE 8000

# Perintah untuk menjalankan aplikasi Anda dengan Uvicorn
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
