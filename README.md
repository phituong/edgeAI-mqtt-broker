# EdgeAI MQTT Broker
This project provides a lightweight anomaly detection system designed for IoT networks. It uses TensorFlow Lite models and deploys them on edge devices that run an MQTT broker, enabling real-time anomaly detection.


## ⚙️ Training models:
Install dependencies and set up TensorFlow environment:
   python setup_tensorflow.py

Generate sample training data:
   python create_sample.py

Train and convert the model to TFLite format:
   python training.py


🚀 Deployment Notes
The files in broker/src/ contain the core MQTT broker implementation that supports EdgeAI processing. These can be deployed to embedded systems (such as those running Zephyr OS). The broker manages MQTT messaging, communicates with IoT devices, and uses the trained models for on-device anomaly detection.

🤖 Trained Models
Located in broker/src/model/. These TFLite models are ready to run on microcontrollers for real-time inference.

🛠️ Modify or Extend
You can customize:
The model architecture in training.py
The data generation logic in create_sample.py
The broker functionality in broker/src/


📜 License
This project is released under the MIT License.
Please contract laulpt@gmail.con for more information.
