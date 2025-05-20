import pandas as pd
import random
from sklearn.cluster import KMeans
import numpy as np
from collections import defaultdict
from datasketch import MinHash, MinHashLSH


def randomize_feature(sample, feature, class_label, ranges):
    """Randomize the feature based on class_label."""
    if class_label == 0:
        sample[feature] = random.choice(ranges[0])  # Range for class_label 0
    elif class_label == 1:
        sample[feature] = random.choice(ranges[1])  # Range for class_label 1
    elif class_label == 2:
        sample[feature] = random.choice(ranges[2])  # Range for class_label 2

def create_sample(event_type, class_label):
    # Initialize the features dictionary with default values
    sample = {
        "Event Type": event_type,
        "Clean Session": -1,
        "Will Flag": -1,
        "QoS/Will QoS": -1,
        "Retain/Will Retain": -1,
        "Payload/Will Payload Size": -1,
        "Keep-Alive": -1,
        "Reconn Frequency": -1,
        "Pending Queue": -1,
        "Retained Queue": -1,
        "Subscription Count": -1,
        "Resubscription Frequency": -1,
        "Republish Frequency": -1,
    }

    # Define feature ranges for class labels
    ranges = {
        "Clean Session": [[0, 1], [0, 1], [0, 1]],
        "Will Flag": [[0, 1], [0, 1], [0, 1]],
        "QoS/Will QoS": [[0, 1, 2], [0, 1, 2], [0, 1, 2]],
        "Retain/Will Retain": [[0, 1], [0, 1], [0, 1]],  # Fix here: retain is just 0 for Will
        "Payload/Will Payload Size": [[0, 1, 2], [0, 1, 2], [0, 1, 2]],
        "Keep-Alive": [[0, 1, 2], [0, 1, 2], [0, 1, 2]],
        "Reconn Frequency": [[0, 1, 2], [0, 1, 2], [0, 1, 2]],
        "Pending Queue": [[0, 1, 2], [0, 1, 2], [0, 1, 2]],
        "Retained Queue": [[0, 1, 2], [0, 1, 2], [0, 1, 2]],
        "Subscription Count": [[0, 1, 2], [0, 1, 2], [0, 1, 2]],
        "Resubscription Frequency": [[0, 1, 2], [0, 1, 2], [0, 1, 2]],
        "Republish Frequency": [[0, 1, 2], [0, 1, 2], [0, 1, 2]]
    }

    if event_type == 1:  # CONNECT
        randomize_feature(sample, 'Clean Session', class_label, ranges['Clean Session'])
        randomize_feature(sample, 'Will Flag', class_label, ranges['Will Flag'])
        
        if sample['Will Flag'] == 1:
            randomize_feature(sample, 'QoS/Will QoS', class_label, ranges['QoS/Will QoS'])
            randomize_feature(sample, 'Retain/Will Retain', class_label, ranges['Retain/Will Retain'])
            randomize_feature(sample, 'Payload/Will Payload Size', class_label, ranges['Payload/Will Payload Size'])
        else:
            sample['QoS/Will QoS'] = -1
            sample['Retain/Will Retain'] = -1
            sample['Payload/Will Payload Size'] = -1
        
        randomize_feature(sample, 'Keep-Alive', class_label, ranges['Keep-Alive'])
        randomize_feature(sample, 'Reconn Frequency', class_label, ranges['Reconn Frequency'])
        randomize_feature(sample, 'Pending Queue', class_label, ranges['Pending Queue'])
        randomize_feature(sample, 'Retained Queue', class_label, ranges['Retained Queue'])
        randomize_feature(sample, 'Subscription Count', class_label, ranges['Subscription Count'])
    
    elif event_type == 2:  # PUBLISH
        randomize_feature(sample, 'QoS/Will QoS', class_label, ranges['QoS/Will QoS'])
        randomize_feature(sample, 'Retain/Will Retain', class_label, ranges['Retain/Will Retain'])
        randomize_feature(sample, 'Payload/Will Payload Size', class_label, ranges['Payload/Will Payload Size'])
        randomize_feature(sample, 'Pending Queue', class_label, ranges['Pending Queue'])
        randomize_feature(sample, 'Retained Queue', class_label, ranges['Retained Queue'])
        randomize_feature(sample, 'Republish Frequency', class_label, ranges['Republish Frequency'])
    
    elif event_type == 3:  # SUBSCRIBE
        randomize_feature(sample, 'QoS/Will QoS', class_label, ranges['QoS/Will QoS'])
        randomize_feature(sample, 'Retained Queue', class_label, ranges['Retained Queue'])
        randomize_feature(sample, 'Resubscription Frequency', class_label, ranges['Resubscription Frequency'])
        randomize_feature(sample, 'Pending Queue', class_label, ranges['Pending Queue'])
        randomize_feature(sample, 'Subscription Count', class_label, ranges['Subscription Count'])

    # Calculate the normalized weight
    normalized_weight = calculate_normalized_weight(sample)
    
    # Add normalized weight to the sample
    sample['normalized_weight'] = normalized_weight
    
    return normalized_weight, sample

def calculate_normalized_weight(sample):
    total_score = 0
    relevant_feature_count = 0
    
    feature_weights = {
        "Event Type": 1.00,
        "Clean Session": 0.67,
        "Will Flag": 0.17,
        "QoS/Will QoS": 1.00,
        "Retain/Will Retain": 1.00,
        "Keep-Alive": 0.50,
        "Payload/Will Payload Size": 1.00,
        "Reconn Frequency": 1.00,
        "Pending Queue": 1.00,
        "Retained Queue": 1.00,
        "Subscription Count": 1.00,
        "Resubscription Frequency": 1.00,
        "Republish Frequency": 1.00,
    }

    # Define how to transform binary/low-value features where lower values mean more impact
    binary_transform = {
        "Clean Session": lambda v: 1 if v == 0 else 0,  # 0 = high impact
        "Will Flag": lambda v: v,                      # 1 = high impact
        # Add more binary-style transformations as needed
    }
    
    # Calculate total score and count relevant features
    for feature, value in sample.items():
        if value == -1:
            continue  # Skip irrelevant features
    
        weight = feature_weights.get(feature, 0)
    
        # Apply special transformation if needed
        if feature in binary_transform:
            transformed_value = binary_transform[feature](value)
            total_score += transformed_value * weight
        else:
            total_score += value * weight
    
        relevant_feature_count += 1

    # Normalize by dividing by the number of relevant features
    if relevant_feature_count > 0:
        normalized_score = total_score / relevant_feature_count
    else:
        normalized_score = 0  # If there are no relevant features, set score to 0

    return normalized_score

# Function to generate a balanced dataset for each event type
def generate_balanced_samples(event_type, n_samples_per_class):
    samples = []
    
    # Generate balanced samples for each class (1, 2, 3) and for each event type
    for class_label in [0, 1, 2]:
        for _ in range(n_samples_per_class):
            sample = create_sample(event_type, class_label)  # Assuming create_sample is defined elsewhere
            samples.append(sample)
    
    return samples

# Example usage: Generate balanced samples for event types CONNECT (1), PUBLISH (2), and SUBSCRIBE (3)
n_samples_per_class = 100  # Number of samples for each class 100, 500, 5000
event_types = [1, 2, 3]  # Event types CONNECT, PUBLISH, SUBSCRIBE

balanced_samples = {}

# Generate balanced samples for each event type
for event_type in event_types:
    balanced_samples[event_type] = generate_balanced_samples(event_type, n_samples_per_class)

# Step 2: Collect all samples into a list with event type and normalized weight
all_samples = []

for event_type, samples in balanced_samples.items():
    for normalized_weight, sample in samples:
        all_samples.append((event_type, normalized_weight, sample))

# Your weights
only_weights = [norm_w for _, norm_w, _ in all_samples]
only_weights = np.array(only_weights)

print("=== Threshold Calculations ===")

# Method 1: Percentiles (33% and 66%)
quantiles = np.percentile(only_weights, [33, 66])
print(f"[Percentile] Thresholds: Low < {quantiles[0]:.4f}, Medium < {quantiles[1]:.4f}, High >= {quantiles[1]:.4f}")

# Method 2: Mean ± Std
mean = np.mean(only_weights)
std = np.std(only_weights)
print(f"[Mean ± Std] Mean: {mean:.4f}, Std: {std:.4f}")
print(f"[Mean ± Std] Thresholds: Low < {mean - std:.4f}, Medium < {mean + std:.4f}, High >= {mean + std:.4f}")

# Method 3: IQR
q1 = np.percentile(only_weights, 25)
q3 = np.percentile(only_weights, 75)
iqr = q3 - q1
print(f"[IQR] Q1: {q1:.4f}, Q3: {q3:.4f}, IQR: {iqr:.4f}")
print(f"[IQR] Thresholds: Outliers < {q1 - 1.5 * iqr:.4f} or > {q3 + 1.5 * iqr:.4f}")

# Method 4: KMeans clustering (3 clusters)
kmeans = KMeans(n_clusters=3, random_state=0)
clusters = kmeans.fit_predict(only_weights.reshape(-1, 1))
centers = sorted(kmeans.cluster_centers_.flatten())
print(f"[KMeans] Cluster Centers (sorted): {centers}")
print(f"[KMeans] Thresholds: Low < {centers[1]:.4f}, Medium < {centers[2]:.4f}, High >= {centers[2]:.4f}")


# Step 3: Define a function to assign labels based on quantiles
def assign_label_based_on_quantiles(weight, quantiles):
    if weight < quantiles[0]:  # Below the first quantile
        return 0  # Low weight (Label 0)
    elif weight < quantiles[1]:  # Between first and second quantile
        return 1  # Borderline weight (Label 1)
    else:
        return 2  # High weight (Label 2)

# Step 4: Assign labels based on quantiles
for _, norm_w, sample in all_samples:
    sample['Label'] = assign_label_based_on_quantiles(norm_w, quantiles)
    sample['normalized_weight'] = norm_w

# Step 6: Prepare data for saving to CSV
data = []
for _, _, sample in all_samples:
    # Flatten the dictionary and add the normalized_weight
    flat_sample = sample.copy()
    flat_sample.pop('normalized_weight', None)  # Remove the 'normalized_weight' key to avoid duplication
    flat_sample['normalized_weight'] = sample['normalized_weight']  # Add normalized weight for CSV
    data.append(flat_sample)

df = pd.DataFrame(data)

# Save to CSV
df.to_csv('sample_data.csv', index=False)

print("Data saved to sample_data.csv")


file_path = "sample_data.csv"  # Change this to your actual file path
df = pd.read_csv(file_path)

### **Step 1: Check for Exact Duplicates** ###
duplicate_count = df.duplicated().sum()
print(f"Total exact duplicate rows: {duplicate_count}")

if duplicate_count > 0:
    print("Exact duplicate rows:")
    print(df[df.duplicated()])

# Remove exact duplicates
df_cleaned = df.drop_duplicates()

### **Step 2: Detect Near-Duplicates using MinHash LSH** ###
# Function to create MinHash signatures
def get_minhash(text, num_perm=128):
    m = MinHash(num_perm=num_perm)
    for word in str(text).split():  # Tokenize by whitespace
        m.update(word.encode('utf8'))
    return m

# Initialize LSH
lsh = MinHashLSH(threshold=0.8, num_perm=128)  # Adjust threshold for similarity
minhashes = {}

# Add MinHash signatures to LSH
for i, row in df_cleaned.iterrows():
    text = " ".join(map(str, row))  # Combine all columns as a single text
    minhash = get_minhash(text)
    lsh.insert(i, minhash)
    minhashes[i] = minhash

# Find near-duplicates
near_duplicates = []
for i, minhash in minhashes.items():
    similar_items = lsh.query(minhash)
    if len(similar_items) > 1:  # More than one means it has near-duplicates
        near_duplicates.append((i, similar_items))

print(f"\nTotal near-duplicates found: {len(near_duplicates)}")

# Save cleaned dataset
cleaned_file_path = "cleaned_dataset.csv"
df_cleaned.to_csv(cleaned_file_path, index=False)
print(f"Cleaned dataset saved as {cleaned_file_path}")