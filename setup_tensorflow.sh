#!/bin/bash

# Exit immediately if any command fails
set -e

echo "🔧 Creating Python 3.11 virtual environment..."
python3.11 -m venv tf-intel

echo "✅ Activating virtual environment..."
source tf-intel/bin/activate

echo "⬆️ Upgrading pip and setuptools..."
pip install --upgrade pip setuptools wheel

echo "📦 Installing TensorFlow 2.13 (Intel-compatible)..."
pip install tensorflow==2.13

echo "➕ Installing additional ML packages (numpy, pandas, scikit-learn, matplotlib)..."
pip install numpy pandas scikit-learn matplotlib jupyterlab

echo "🧪 Verifying TensorFlow installation..."
python -c "import tensorflow as tf; print('✅ TensorFlow version:', tf.__version__)"

echo "🎉 Setup complete! To activate later: source tf-intel/bin/activate"
