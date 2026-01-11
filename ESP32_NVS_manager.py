# /// script
# dependencies = [ "pyserial" ]
# ///

import serial
import serial.tools.list_ports
import json
import tkinter as tk
from tkinter import messagebox, ttk
import time

class NVSManager:
    def __init__(self, root):
        self.root = root
        self.root.title("ESP32 NVS Manager")
        self.ser = None
        self.param_data = [] # Stores schema info

        # --- Connection UI ---
        conn_frame = tk.Frame(root)
        conn_frame.pack(pady=10)
        self.port_combo = ttk.Combobox(conn_frame, values=[p.device for p in serial.tools.list_ports.comports()])
        self.port_combo.pack(side="left", padx=5)
        self.btn_connect = tk.Button(conn_frame, text="Connect", command=self.toggle_conn)
        self.btn_connect.pack(side="left")

        # --- Parameters UI ---
        self.main_frame = tk.LabelFrame(root, text="Device Parameters")
        self.main_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.btn_save = tk.Button(root, text="Save All", command=self.save_all, state="disabled", bg="green", fg="white")
        self.btn_save.pack(pady=10)

    def toggle_conn(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.btn_connect.config(text="Connect")
        else:
            try:
                self.ser = serial.Serial(self.port_combo.get(), 115200, timeout=2)
                time.sleep(2) # Wait for ESP32 boot
                self.btn_connect.config(text="Disconnect")
                self.fetch_schema()
            except Exception as e:
                messagebox.showerror("Error", str(e))

    def fetch_schema(self):
        self.ser.write(b"GET_SCHEMA\n")
        line = self.ser.readline().decode().strip()
        try:
            self.param_data = json.loads(line)
            self.build_ui()
            self.btn_save.config(state="normal")
        except:
            messagebox.showerror("Error", "Failed to load schema.")

    def build_ui(self):
        for widget in self.main_frame.winfo_children(): widget.destroy()
        
        for i, p in enumerate(self.param_data):
            tk.Label(self.main_frame, text=p['label'], width=15, anchor="e").grid(row=i, column=0, padx=5, pady=2)
            
            ent = tk.Entry(self.main_frame)
            ent.insert(0, str(p['val']))
            ent.grid(row=i, column=1, padx=5, pady=2)
            p['entry_widget'] = ent # Link widget to schema object
            
            hint = f"({p['min']} to {p['max']})" if p['type'] == "int" else ""
            tk.Label(self.main_frame, text=hint, fg="gray").grid(row=i, column=2, padx=5)

    def save_all(self):
        for p in self.param_data:
            val_raw = p['entry_widget'].get()
            
            # Validation Logic
            if p['type'] == "int":
                try:
                    val = int(val_raw)
                    if not (p['min'] <= val <= p['max']):
                        raise ValueError(f"{p['label']} must be {p['min']}-{p['max']}")
                except ValueError as e:
                    messagebox.showerror("Validation Error", str(e))
                    return
            else:
                val = val_raw

            # Send to ESP32
            cmd = f"SET_VAR:{json.dumps({'key': p['key'], 'val': val})}\n"
            self.ser.write(cmd.encode())
            res = self.ser.readline().decode().strip()
            if "OK" not in res:
                messagebox.showerror("ESP32 Error", f"Failed to save {p['key']}: {res}")
                return
        
        messagebox.showinfo("Success", "All settings saved and applied!")

if __name__ == "__main__":
    root = tk.Tk()
    NVSManager(root)
    root.mainloop()