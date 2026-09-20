import numpy as np
import random

# Actions: 0=10Hz (Ultra-low power), 1=50Hz (Standard), 2=250Hz (High-Fi), 3=500Hz (Burst)
ACTIONS = [10, 50, 250, 500]
NUM_ACTIONS = len(ACTIONS)

# States: 0=Normal, 1=Elevated, 2=Critical, 3=Lead_Off
NUM_STATES = 4

# Initialize Q-table
q_table = np.zeros((NUM_STATES, NUM_ACTIONS))

# Hyperparameters
alpha = 0.1   # Learning rate
gamma = 0.9   # Discount factor
epsilon = 0.2 # Exploration rate

def get_reward(state, action_idx):
    # Reward matrix penalizing high power when normal, penalizing low sampling during critical events
    if state == 3: # Lead off
        return 10.0 if action_idx == 0 else -5.0
    elif state == 2: # Critical
        return 10.0 if action_idx == 3 else -10.0
    elif state == 1: # Elevated
        return 5.0 if action_idx == 2 else -2.0
    else: # Normal
        return 5.0 if action_idx == 1 else (2.0 if action_idx == 0 else -3.0)

# Q-Learning Loop
print("Training Q-Learning model for BIOMED-RL policy...")
for episode in range(1000):
    state = random.randint(0, NUM_STATES - 1)
    
    if random.uniform(0, 1) < epsilon:
        action = random.randint(0, NUM_ACTIONS - 1)
    else:
        action = np.argmax(q_table[state])
        
    reward = get_reward(state, action)
    next_state = random.randint(0, NUM_STATES - 1)
    
    q_table[state, action] = q_table[state, action] + alpha * (reward + gamma * np.max(q_table[next_state]) - q_table[state, action])

print("\nFinal Trained Q-Table:")
print(q_table)
