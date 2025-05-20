# EdgeAI MQTT Broker

This project provides a lightweight anomaly detection system designed for IoT networks. It uses TensorFlow Lite models and deploys them on edge devices that run an MQTT broker, enabling real-time anomaly detection.

## 📁 Project Structure

edgeAI-mqtt-broker/
│
├── broker/
│ └── src/
│ ├── model/ # Trained TFLite models for edge deployment
│ └── ... # Source code files for the MQTT broker
│
├── setup_tensorflow.py # Sets up the TensorFlow environment for microcontrollers
├── create_sample.py # Generates random sample data for training
├── training.py # Trains ML models and converts them to TFLite format

bash
Copy
Edit

## ⚙️ Setup Instructions

1. **Create and activate a virtual environment:**
   ```bash
   python3 -m venv venv
   source venv/bin/activate
Install dependencies and set up TensorFlow environment:

bash
Copy
Edit
pip install -r requirements.txt
python setup_tensorflow.py
Generate sample training data:

bash
Copy
Edit
python create_sample.py
Train and convert the model to TFLite format:

bash
Copy
Edit
python training.py
🚀 Deployment Notes
The files in broker/src/ contain the core MQTT broker implementation that supports EdgeAI processing.

These can be deployed to embedded systems (such as those running Zephyr OS).

The broker manages MQTT messaging, communicates with IoT devices, and uses the trained models for on-device anomaly detection.

🤖 Trained Models
Located in broker/src/model/

These TFLite models are ready to run on microcontrollers for real-time inference.

🛠️ Modify or Extend
You can customize:

The model architecture in training.py

The data generation logic in create_sample.py

The broker functionality in broker/src/

📜 License
This project is released under the MIT License.
