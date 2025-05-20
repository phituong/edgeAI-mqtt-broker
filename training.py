import pandas as pd
import numpy as np
import tensorflow as tf
from tensorflow import keras
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler, LabelEncoder
from sklearn.metrics import classification_report, precision_score, recall_score, f1_score, roc_auc_score, average_precision_score
from datetime import datetime
import os
from sklearn.utils.class_weight import compute_class_weight
from sklearn.metrics import confusion_matrix, ConfusionMatrixDisplay
import matplotlib.pyplot as plt

# Load dataset
df = pd.read_csv("cleaned_dataset_6000.csv")

# Separate features and labels
X = df.drop(columns=["Label"])
y = df["Label"]

# Suppose your label column is like this:
labels = df['Label'].values

# Compute class weights for labels 0, 1, 2
class_weights = compute_class_weight(class_weight='balanced', classes=np.unique(labels), y=labels)

# Convert to dictionary for model usage
class_weight_dict = dict(zip(np.unique(labels), class_weights))

# Encode labels
label_encoder = LabelEncoder()
y = label_encoder.fit_transform(y)

# Split data
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# Scale features
scaler = StandardScaler()
X_train = scaler.fit_transform(X_train)
X_test = scaler.transform(X_test)

# Define a small neural network model (DNN)
model = keras.Sequential([
    keras.layers.Dense(16, activation='relu', input_shape=(13,)),   # input: 13 features
    keras.layers.Dense(8, activation='relu'),
    keras.layers.Dense(3, activation='softmax')                     # output: 3 classes
])

model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])

# Train the model
model.fit(X_train, y_train, epochs=50, batch_size=16, validation_data=(X_test, y_test), class_weight=class_weight_dict)

# -------- Convert model to TFLite and prepare for Zephyr --------
def representative_data_gen():
    for i in range(100):
        yield [X_train[i:i+1].astype(np.float32)]


def convert_to_tflite_int8(model, filename="model_quantized_int8.tflite"):
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_data_gen
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]

    # These two lines force fully integer model — critical for TFLM
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()

    with open(filename, "wb") as f:
        f.write(tflite_model)
    print(f"Quantized INT8 model saved as {filename}")
    return filename


def convert_to_tflite_float(model, filename="model_float.tflite"):
    converter = tf.lite.TFLiteConverter.from_keras_model(model)

    # No optimizations, no quantization, no need to specify supported_ops
    tflite_model = converter.convert()

    with open(filename, "wb") as f:
        f.write(tflite_model)
    print(f"Float model saved as {filename}")
    return filename


# Convert to TFLite
#tflite_filename = convert_to_tflite_int8(model)
tflite_filename = convert_to_tflite_float(model)


# Convert .tflite to C array for Zephyr
def tflite_to_c_array(tflite_path, header_path="model_data.h", array_name="model_data"):
    with open(tflite_path, "rb") as f:
        data = f.read()
    with open(header_path, "w") as f:
        f.write(f"#ifndef MODEL_DATA_H\n#define MODEL_DATA_H\n\n")
        f.write(f"#include <stddef.h>\n\n")
        f.write(f"const unsigned char {array_name}[] = {{\n")
        for i, byte in enumerate(data):
            if i % 12 == 0:
                f.write("\n ")
            f.write(f"0x{byte:02x}, ")
        f.write("\n};\n")
        f.write(f"const size_t {array_name}_len = sizeof({array_name});\n\n")
        f.write(f"#endif // MODEL_DATA_H\n")
    print(f"C header saved to {header_path}")

# Generate C array header
tflite_to_c_array(tflite_filename)

# --------- Continue model evaluation ---------
# Predictions
y_pred = np.argmax(model.predict(X_test), axis=1)
y_pred_prob = model.predict(X_test)

# Scores (Overall Performance)
prec = precision_score(y_test, y_pred, average='weighted')
rec = recall_score(y_test, y_pred, average='weighted')
f1 = f1_score(y_test, y_pred, average='weighted')
roc_auc = roc_auc_score(y_test, y_pred_prob, multi_class='ovr', average='weighted')
pr_auc = average_precision_score(y_test, y_pred_prob, average='weighted')

# Classification report (Per-Class)
report_dict = classification_report(y_test, y_pred, output_dict=True)
report_df = pd.DataFrame(report_dict).transpose()
report_df["timestamp"] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

# Overall performance as a separate DataFrame
overall_metrics = {
    "Metric": ["Precision", "Recall", "F1-Score", "ROC AUC", "PR AUC"],
    "Value": [prec, rec, f1, roc_auc, pr_auc],
    "timestamp": [datetime.now().strftime("%Y-%m-%d %H:%M:%S")] * 5
}
overall_metrics_df = pd.DataFrame(overall_metrics)

# Save to Excel
excel_file = "classification_results.xlsx"
if not os.path.exists(excel_file):
    with pd.ExcelWriter(excel_file, engine='openpyxl', mode='w') as writer:
        overall_metrics_df.to_excel(writer, sheet_name='Results', startrow=0, index=False)
        report_df.to_excel(writer, sheet_name='Results', startrow=8)  # Leave some space
else:
    with pd.ExcelWriter(excel_file, engine='openpyxl', mode='a', if_sheet_exists='overlay') as writer:
        existing_df = pd.read_excel(excel_file, sheet_name='Results')
        start_row = existing_df.shape[0] + 2
        overall_metrics_df.to_excel(writer, sheet_name='Results', startrow=start_row, index=False)
        report_df.to_excel(writer, sheet_name='Results', startrow=start_row + 8)

print(f"Classification report with overall performance saved to {excel_file}")


# Plot Confusion Matrix
cm = confusion_matrix(y_test, y_pred)
disp = ConfusionMatrixDisplay(confusion_matrix=cm, display_labels=label_encoder.classes_)
disp.plot(cmap=plt.cm.Blues)
plt.title("Confusion Matrix")
plt.show()

# Final metrics
print(f"Precision: {prec}")
print(f"Recall: {rec}")
print(f"F1-Score: {f1}")
print(f"ROC AUC (macro): {roc_auc}")
print(f"PR AUC (macro): {pr_auc}")
