# EdgeAI MQTT Broker
This project provides a lightweight anomaly detection system designed for IoT networks. It uses TensorFlow Lite models and deploys them on edge devices that run an MQTT broker, enabling real-time anomaly detection.

Install dependencies and set up TensorFlow environment:
   ./setup_tensorflow.sh

Generate sample training data:
   python create_sample.py

Train and convert the model to TFLite format:
   python training.py

🚀 Deployment Notes
- The files in broker/src/ contain the core MQTT broker implementation that supports EdgeAI processing. These can be deployed to embedded systems (such as those running Zephyr OS). The broker manages MQTT messaging, communicates with IoT devices, and uses the trained models for on-device anomaly detection.

🤖 Trained Models
- Located in broker/src/model/. These TFLite models are ready to run on microcontrollers for real-time inference.

📜 License
- This project is released under the MIT License.
- Please contact laulpt@gmail.con for more information.
