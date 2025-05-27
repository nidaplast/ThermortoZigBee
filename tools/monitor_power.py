#!/usr/bin/env python3
"""
ThermortoZigBee Power Monitoring Tool
Monitors and logs power consumption data from the radiator
"""

import sys
import time
import json
import argparse
from datetime import datetime
import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
from collections import deque
import threading

class PowerMonitor:
    def __init__(self, mqtt_host="localhost", mqtt_port=1883, device_name="thermor_salon"):
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.device_name = device_name
        self.topic = f"zigbee2mqtt/{device_name}"
        
        # Data storage
        self.power_history = deque(maxlen=3600)  # 1 hour at 1 sample/sec
        self.temp_history = deque(maxlen=3600)
        self.timestamps = deque(maxlen=3600)
        
        # Statistics
        self.total_energy = 0  # Wh
        self.max_power = 0
        self.start_time = time.time()
        
        # MQTT client
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
        # Lock for thread safety
        self.data_lock = threading.Lock()
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"✓ Connected to MQTT broker at {self.mqtt_host}:{self.mqtt_port}")
            client.subscribe(self.topic)
            print(f"✓ Subscribed to {self.topic}")
        else:
            print(f"✗ Failed to connect, return code {rc}")
            
    def on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
            
            with self.data_lock:
                # Extract power data
                if 'power' in payload:
                    power = float(payload['power'])
                    self.power_history.append(power)
                    self.max_power = max(self.max_power, power)
                    
                    # Calculate energy (Wh)
                    if len(self.power_history) > 1:
                        self.total_energy += power / 3600  # 1 second sample
                        
                # Extract temperature data
                if 'local_temperature' in payload:
                    temp = float(payload['local_temperature'])
                    self.temp_history.append(temp)
                    
                # Add timestamp
                self.timestamps.append(time.time())
                
                # Print current values
                self.print_status(power if 'power' in payload else 0)
                
        except json.JSONDecodeError:
            print(f"✗ Invalid JSON: {msg.payload}")
        except Exception as e:
            print(f"✗ Error processing message: {e}")
            
    def print_status(self, current_power):
        runtime = time.time() - self.start_time
        hours = runtime / 3600
        
        print(f"\r[{datetime.now().strftime('%H:%M:%S')}] "
              f"Power: {current_power:6.1f}W | "
              f"Max: {self.max_power:6.1f}W | "
              f"Energy: {self.total_energy:7.2f}Wh | "
              f"Avg: {self.total_energy/hours if hours > 0 else 0:6.1f}W | "
              f"Cost: {self.total_energy * 0.15 / 1000:5.2f}€", end='')
              
    def start(self):
        """Start monitoring"""
        print("Starting ThermortoZigBee Power Monitor")
        print("Press Ctrl+C to stop and show graphs")
        print("-" * 80)
        
        self.client.connect(self.mqtt_host, self.mqtt_port, 60)
        
        try:
            self.client.loop_forever()
        except KeyboardInterrupt:
            print("\n\nStopping monitor...")
            self.client.disconnect()
            self.show_graphs()
            
    def show_graphs(self):
        """Display collected data as graphs"""
        if len(self.timestamps) < 2:
            print("Not enough data to display graphs")
            return
            
        with self.data_lock:
            # Convert timestamps to relative time in minutes
            start_time = self.timestamps[0]
            time_minutes = [(t - start_time) / 60 for t in self.timestamps]
            
            # Create figure with subplots
            fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
            fig.suptitle(f'ThermortoZigBee Power Monitor - {self.device_name}', fontsize=16)
            
            # Power graph
            ax1.plot(time_minutes, list(self.power_history), 'b-', linewidth=1)
            ax1.set_ylabel('Power (W)', fontsize=12)
            ax1.grid(True, alpha=0.3)
            ax1.fill_between(time_minutes, list(self.power_history), alpha=0.3)
            
            # Add average line
            if self.power_history:
                avg_power = sum(self.power_history) / len(self.power_history)
                ax1.axhline(y=avg_power, color='r', linestyle='--', 
                           label=f'Average: {avg_power:.1f}W')
                ax1.legend()
            
            # Temperature graph
            if self.temp_history:
                ax2.plot(time_minutes, list(self.temp_history), 'r-', linewidth=2)
                ax2.set_ylabel('Temperature (°C)', fontsize=12)
                ax2.set_xlabel('Time (minutes)', fontsize=12)
                ax2.grid(True, alpha=0.3)
            
            # Add statistics text box
            stats_text = (
                f"Total Energy: {self.total_energy:.2f} Wh\n"
                f"Max Power: {self.max_power:.1f} W\n"
                f"Duration: {(self.timestamps[-1] - self.timestamps[0])/3600:.2f} hours\n"
                f"Est. Cost: {self.total_energy * 0.15 / 1000:.3f} €"
            )
            
            fig.text(0.02, 0.02, stats_text, transform=fig.transFigure, 
                    fontsize=10, verticalalignment='bottom',
                    bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
            
            plt.tight_layout()
            
            # Save graph
            filename = f"power_log_{self.device_name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.png"
            plt.savefig(filename, dpi=150)
            print(f"\n✓ Graph saved as {filename}")
            
            plt.show()
            
    def save_data(self, filename=None):
        """Save collected data to JSON file"""
        if filename is None:
            filename = f"power_data_{self.device_name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            
        with self.data_lock:
            data = {
                'device': self.device_name,
                'start_time': datetime.fromtimestamp(self.start_time).isoformat(),
                'end_time': datetime.now().isoformat(),
                'total_energy_wh': self.total_energy,
                'max_power_w': self.max_power,
                'samples': {
                    'timestamps': list(self.timestamps),
                    'power': list(self.power_history),
                    'temperature': list(self.temp_history)
                }
            }
            
        with open(filename, 'w') as f:
            json.dump(data, f, indent=2)
            
        print(f"✓ Data saved to {filename}")

def main():
    parser = argparse.ArgumentParser(description='ThermortoZigBee Power Monitor')
    parser.add_argument('--host', default='localhost', help='MQTT broker host')
    parser.add_argument('--port', type=int, default=1883, help='MQTT broker port')
    parser.add_argument('--device', default='thermor_salon', help='Device name in Zigbee2MQTT')
    parser.add_argument('--save', action='store_true', help='Save data to file on exit')
    
    args = parser.parse_args()
    
    monitor = PowerMonitor(args.host, args.port, args.device)
    
    try:
        monitor.start()
    finally:
        if args.save:
            monitor.save_data()

if __name__ == "__main__":
    main()