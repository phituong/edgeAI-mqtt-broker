# EdgeAI MQTT Broker
This project provides a lightweight anomaly detection system designed for IoT networks. It uses TensorFlow Lite models and deploys them on edge devices that run an MQTT broker, enabling real-time anomaly detection.

Install dependencies and set up TensorFlow environment:
   ./setup_tensorflow.sh

Generate sample training data:
   python create_sample.py

Train and convert the model to TFLite format:
   python training.py

Test script runs on a computer as a mqtt client to send messages to the broker:
connect_broker.py, subscribe.py, unsub.py

🚀 Deployment Notes
- The files in broker/src/ contain the core MQTT broker implementation that supports EdgeAI processing. These can be deployed to embedded systems (such as those running Zephyr OS). The broker manages MQTT messaging, communicates with IoT devices, and uses the trained models for on-device anomaly detection.

🤖 Trained Models
- Located in broker/src/model/. These TFLite models are ready to run on microcontrollers for real-time inference.

Confusion Matrix for the int8 and float32 models at different dataset sizes (int8 at first column, float32 at second column):
- Dataset size = 668 instances
<p align="center">
  <img src="/668_int8.png" alt="Architecture Diagram" width="300"/>
  <img src="/668_float.png" alt="Architecture Diagram" width="300"/>
</p>

- Dataset size = 1815 instances
<p align="center">
  <img src="/1815_int8.png" alt="Architecture Diagram" width="300"/>
  <img src="/1815_float.png" alt="Architecture Diagram" width="300"/>
</p>

- Dataset size = 6245 instances
<p align="center">
  <img src="/6245_int8.png" alt="Architecture Diagram" width="300"/>
  <img src="/6245_float.png" alt="Architecture Diagram" width="300"/>
</p>


📜 License
- This project is released under the MIT License.
- For inquiries, please contact laulpt@gmail.com.
- This project is described in the publication:
Lau, P.T., and Katzenbeisser, S., "Detecting Anomalous Resource Consumption in EdgeAI-Based MQTT Brokers," in Proceedings of the 11th Workshop on the Security of Industrial Control Systems & Cyber-Physical Systems (CyberICPS) in conjunction with ESORICS, 2025.

