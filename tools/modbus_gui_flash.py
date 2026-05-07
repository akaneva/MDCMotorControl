import tkinter as tk
from tkinter import ttk, messagebox
from pymodbus.client import ModbusTcpClient
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk

# ==============================================================================
# MODBUS REGISTER MAP CONSTANTS (Matches STM32 Firmware)
# ==============================================================================
REG_ENCODER_POS   = 100
REG_MOTOR_STATE   = 101
REG_CYCLE_TIME    = 102  
REG_CYCLE_COUNTER = 103

REG_MOTOR_CTRL    = 200
REG_MOTOR_DIR     = 201
REG_MOTOR_RPM     = 202
REG_MOTOR_ACCEL   = 203
REG_MOTOR_MICROSTEPS = 204  # <--- NEW: Microsteps register

REG_STREAM_CTRL   = 300
REG_ENCODER_DIR   = 301
REG_UART_BAUD     = 302
REG_AUTO_STREAM   = 303  

REG_IP_HIGH       = 310
REG_IP_LOW        = 311
REG_NETMASK_HIGH  = 312
REG_NETMASK_LOW   = 313
REG_GATEWAY_HIGH  = 314
REG_GATEWAY_LOW   = 315

REG_SAVE_SETTINGS = 400
REG_SYSTEM_REBOOT = 401

MOTOR_STATES = ["STOPPED", "ACCELERATING", "CRUISING", "DECELERATING"]

class MotorControlApp:
    def __init__(self, root):
        self.root = root
        self.root.title("STM32 Industrial Controller")
        self.root.geometry("700x900") 
        self.root.resizable(True, True)
        
        self.client = None
        self.is_connected = False
        self.poll_job = None
        
        self.last_cycle_time = -1 
        self.last_enc_pos = -1
        self.cycle_history = [] 
        self.stream_events = [] # Списък, който пази (индекс, цвят) за вертикалните линии
        
        self.build_ui()

    def build_ui(self):
        padding = {'padx': 10, 'pady': 5}
        
        frame_conn = ttk.LabelFrame(self.root, text="Network Connection")
        frame_conn.pack(fill="x", padx=10, pady=10)
        
        ttk.Label(frame_conn, text="Board IP:").grid(row=0, column=0, **padding)
        self.conn_ip_var = tk.StringVar(value="192.168.1.151")
        ttk.Entry(frame_conn, textvariable=self.conn_ip_var, width=15).grid(row=0, column=1, **padding)
        
        self.btn_connect = ttk.Button(frame_conn, text="Connect", command=self.toggle_connection)
        self.btn_connect.grid(row=0, column=2, **padding)

        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, padx=10, pady=5)

        self.tab_ctrl = ttk.Frame(self.notebook)
        self.tab_cfg = ttk.Frame(self.notebook)
        self.tab_graph = ttk.Frame(self.notebook) 
        
        self.notebook.add(self.tab_ctrl, text="Control & Telemetry")
        self.notebook.add(self.tab_cfg, text="System Settings")
        self.notebook.add(self.tab_graph, text="Cycle Time Graph")

        self.build_control_tab(padding)
        self.build_settings_tab(padding)
        self.build_graph_tab()

    def build_control_tab(self, padding):
        frame_ctrl = ttk.LabelFrame(self.tab_ctrl, text="Execution Control")
        frame_ctrl.pack(fill="x", padx=10, pady=10)
        
        self.btn_start_motor = ttk.Button(frame_ctrl, text="▶ START MOTOR", command=lambda: self.write_register(REG_MOTOR_CTRL, 1), state="disabled")
        self.btn_start_motor.grid(row=0, column=0, padx=20, pady=10)
        
        self.btn_stop_motor = ttk.Button(frame_ctrl, text="⏹ STOP MOTOR", command=lambda: self.write_register(REG_MOTOR_CTRL, 0), state="disabled")
        self.btn_stop_motor.grid(row=0, column=1, padx=20, pady=10)
        
        # Обновени бутони: използват нови функции за запис на събития
        self.btn_start_stream = ttk.Button(frame_ctrl, text="Start UART Stream", command=self.action_start_stream, state="disabled")
        self.btn_start_stream.grid(row=1, column=0, padx=20, pady=5)
        
        self.btn_stop_stream = ttk.Button(frame_ctrl, text="Stop UART Stream", command=self.action_stop_stream, state="disabled")
        self.btn_stop_stream.grid(row=1, column=1, padx=20, pady=5)

        frame_telemetry = ttk.LabelFrame(self.tab_ctrl, text="Live Telemetry")
        frame_telemetry.pack(fill="both", expand=True, padx=10, pady=10)
        
        frame_telemetry.columnconfigure(0, weight=1, minsize=150) 
        frame_telemetry.columnconfigure(1, weight=1, minsize=150) 
        frame_telemetry.columnconfigure(2, weight=0, minsize=50)  

        ttk.Label(frame_telemetry, text="Encoder Position:", font=("Arial", 12, "bold")).grid(row=0, column=0, sticky="w", **padding)
        self.lbl_enc_pos = ttk.Label(frame_telemetry, text="0", font=("Arial", 14), foreground="blue", width=15)
        self.lbl_enc_pos.grid(row=0, column=1, sticky="w", **padding)
        
        self.enc_led_canvas = tk.Canvas(frame_telemetry, width=20, height=20, highlightthickness=0, borderwidth=0, relief="flat")
        self.enc_led_canvas.grid(row=0, column=2, padx=10, sticky="e") 
        self.led_enc = self.enc_led_canvas.create_oval(2, 2, 18, 18, fill="gray30")
        
        ttk.Label(frame_telemetry, text="Motor State:", font=("Arial", 12, "bold")).grid(row=1, column=0, sticky="w", **padding)
        self.lbl_motor_state = ttk.Label(frame_telemetry, text="UNKNOWN", font=("Arial", 14), foreground="green", width=15)
        self.lbl_motor_state.grid(row=1, column=1, sticky="w", **padding)

        self.state_led_canvas = tk.Canvas(frame_telemetry, width=20, height=20, highlightthickness=0, borderwidth=0, relief="flat")
        self.state_led_canvas.grid(row=1, column=2, padx=10, sticky="e") 
        self.led_state = self.state_led_canvas.create_oval(2, 2, 18, 18, fill="gray30")

        ttk.Label(frame_telemetry, text="Rev. Cycle Time:", font=("Arial", 12, "bold")).grid(row=2, column=0, sticky="w", **padding)
        self.lbl_cycle_time = ttk.Label(frame_telemetry, text="0 ms", font=("Arial", 14), foreground="red", width=15)
        self.lbl_cycle_time.grid(row=2, column=1, sticky="w", **padding)

        self.cycle_led_canvas = tk.Canvas(frame_telemetry, width=20, height=20, highlightthickness=0, borderwidth=0, relief="flat")
        self.cycle_led_canvas.grid(row=2, column=2, padx=10, sticky="e") 
        self.led_cycle = self.cycle_led_canvas.create_oval(2, 2, 18, 18, fill="gray30")
       
        # Row 3: Cycle Counter (FIXED)
        ttk.Label(frame_telemetry, text="Cycle Counter:", font=("Arial", 12, "bold")).grid(row=3, column=0, sticky="w", **padding)
        self.lbl_cycle_count = ttk.Label(frame_telemetry, text="0", font=("Arial", 14), foreground="purple")
        self.lbl_cycle_count.grid(row=3, column=1, sticky="w", **padding)

        self.canvas_count = tk.Canvas(frame_telemetry, width=20, height=20, highlightthickness=0, borderwidth=0, relief="flat")
        self.canvas_count.grid(row=3, column=2, padx=10, sticky="e") 
        self.led_count = self.canvas_count.create_oval(2, 2, 18, 18, fill="gray30")



    def build_settings_tab(self, padding):
        frame_motor = ttk.LabelFrame(self.tab_cfg, text="Motor & System Parameters")
        frame_motor.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(frame_motor, text="Target RPM:").grid(row=0, column=0, sticky="w", **padding)
        self.rpm_var = tk.StringVar(value="10.0")
        ttk.Entry(frame_motor, textvariable=self.rpm_var, width=10).grid(row=0, column=1, **padding)
        
        ttk.Label(frame_motor, text="Accel Time (s):").grid(row=1, column=0, sticky="w", **padding)
        self.accel_var = tk.StringVar(value="2.0")
        ttk.Entry(frame_motor, textvariable=self.accel_var, width=10).grid(row=1, column=1, **padding)
        
        ttk.Label(frame_motor, text="Motor Direction (0=Fwd, 1=Rev):").grid(row=2, column=0, sticky="w", **padding)
        self.motor_dir_var = tk.IntVar(value=0)
        ttk.Combobox(frame_motor, textvariable=self.motor_dir_var, values=[0, 1], state="readonly", width=8).grid(row=2, column=1, **padding)
        
        ttk.Label(frame_motor, text="Encoder Dir (0=Norm, 1=Rev):").grid(row=3, column=0, sticky="w", **padding)
        self.enc_dir_var = tk.IntVar(value=0)
        ttk.Combobox(frame_motor, textvariable=self.enc_dir_var, values=[0, 1], state="readonly", width=8).grid(row=3, column=1, **padding)
        
        # <--- NEW: Microsteps Entry --->
        ttk.Label(frame_motor, text="Microsteps:").grid(row=4, column=0, sticky="w", **padding)
        self.microsteps_var = tk.StringVar(value="5000")
        ttk.Entry(frame_motor, textvariable=self.microsteps_var, width=10).grid(row=4, column=1, **padding)

        ttk.Label(frame_motor, text="UART Baud Rate:").grid(row=5, column=0, sticky="w", **padding)
        self.baud_var = tk.StringVar(value="1000000")
        ttk.Entry(frame_motor, textvariable=self.baud_var, width=10).grid(row=5, column=1, **padding)

        self.auto_stream_var = tk.IntVar(value=0)
        ttk.Checkbutton(frame_motor, text="Auto-Start Telemetry Stream on Boot", variable=self.auto_stream_var).grid(row=6, column=0, columnspan=2, sticky="w", padx=10, pady=10)

        frame_net = ttk.LabelFrame(self.tab_cfg, text="Network Configuration (Requires Reboot)")
        frame_net.pack(fill="x", padx=10, pady=5)

        ttk.Label(frame_net, text="Board IP Address:").grid(row=0, column=0, sticky="w", **padding)
        self.cfg_ip_var = tk.StringVar(value="192.168.1.151")
        ttk.Entry(frame_net, textvariable=self.cfg_ip_var, width=15).grid(row=0, column=1, **padding)

        ttk.Label(frame_net, text="Subnet Mask:").grid(row=1, column=0, sticky="w", **padding)
        self.cfg_mask_var = tk.StringVar(value="255.255.255.0")
        ttk.Entry(frame_net, textvariable=self.cfg_mask_var, width=15).grid(row=1, column=1, **padding)

        ttk.Label(frame_net, text="Gateway:").grid(row=2, column=0, sticky="w", **padding)
        self.cfg_gw_var = tk.StringVar(value="192.168.1.1")
        ttk.Entry(frame_net, textvariable=self.cfg_gw_var, width=15).grid(row=2, column=1, **padding)

        frame_actions = ttk.Frame(self.tab_cfg)
        frame_actions.pack(fill="x", padx=10, pady=10)

        self.btn_load = ttk.Button(frame_actions, text="Read from Board", command=self.load_parameters, state="disabled")
        self.btn_load.grid(row=0, column=0, padx=5, pady=5)

        self.btn_apply = ttk.Button(frame_actions, text="Apply to RAM", command=self.apply_parameters, state="disabled")
        self.btn_apply.grid(row=0, column=1, padx=5, pady=5)

        self.btn_save = ttk.Button(frame_actions, text="Save to Flash", command=self.save_to_flash, state="disabled")
        self.btn_save.grid(row=1, column=0, padx=5, pady=5)

        self.btn_reboot = ttk.Button(frame_actions, text="Reboot Board", command=self.reboot_board, state="disabled")
        self.btn_reboot.grid(row=1, column=1, padx=5, pady=5)

    def build_graph_tab(self):
        """ Изгражда таба за графика, който се разпъва динамично. """
        frame_g_ctrl = ttk.Frame(self.tab_graph)
        frame_g_ctrl.pack(fill="x", padx=10, pady=5)
        
        ttk.Button(frame_g_ctrl, text="🗑 Clear Data", command=self.clear_graph).pack(side="left", padx=5)
        
        self.auto_scroll_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(frame_g_ctrl, text="Auto-Scroll to Newest", variable=self.auto_scroll_var).pack(side="left", padx=10)

        frame_canvas = ttk.Frame(self.tab_graph)
        frame_canvas.pack(fill="both", expand=True, padx=10, pady=5)

        self.fig, self.ax = plt.subplots(figsize=(5, 4), dpi=100)
        self.fig.patch.set_facecolor('#F0F0F0') 
        
        self.ax.set_title("Live Cycle Time History")
        self.ax.set_xlabel("Update Sequence")
        self.ax.set_ylabel("Time (ms)")
        self.ax.grid(True, linestyle='--', alpha=0.7)

        self.line, = self.ax.plot([], [], color='red', marker='o', markersize=5, linewidth=1.5)

        self.annot = self.ax.annotate("", xy=(0,0), xytext=(10,10), textcoords="offset points",
                                      bbox=dict(boxstyle="round", fc="yellow", alpha=0.9),
                                      arrowprops=dict(arrowstyle="->"))
        self.annot.set_visible(False)

        self.canvas_graph = FigureCanvasTkAgg(self.fig, master=frame_canvas)
        self.canvas_graph.draw()
        
        self.toolbar = NavigationToolbar2Tk(self.canvas_graph, frame_canvas)
        self.toolbar.update()
        
        self.canvas_graph.get_tk_widget().pack(fill="both", expand=True)

        self.fig.canvas.mpl_connect("motion_notify_event", self.on_hover)

    def on_hover(self, event):
        vis = self.annot.get_visible()
        if event.inaxes == self.ax:
            cont, ind = self.line.contains(event)
            if cont:
                pos = self.line.get_xydata()[ind["ind"][0]]
                self.annot.xy = pos
                text = f"Val: {pos[1]:.1f} ms"
                self.annot.set_text(text)
                self.annot.set_visible(True)
                self.canvas_graph.draw_idle()
            else:
                if vis:
                    self.annot.set_visible(False)
                    self.canvas_graph.draw_idle()

    def clear_graph(self):
        self.cycle_history.clear()
        self.stream_events.clear() # Изчистваме и маркерите
        self.line.set_data([], [])
        self.ax.relim()
        self.ax.autoscale_view()
        self.canvas_graph.draw_idle()

    def update_graph(self):
        """ Обновява графиката с данни и вертикални маркери. """
        if not self.cycle_history:
            return
            
        y_data = list(self.cycle_history)
        x_data = list(range(len(y_data)))
        self.line.set_data(x_data, y_data)
        
        # Премахваме старите вертикални линии преди повторно изчертаване (за оптимизация)
        for line in list(self.ax.get_lines()):
            if line is not self.line:
                line.remove()

        # Чертаем запазените събития (Старт/Стоп стрийм)
        for index, color in self.stream_events:
            self.ax.axvline(x=index, color=color, linestyle='--', linewidth=1.5, alpha=0.7)

        if self.auto_scroll_var.get():
            window = 100
            if len(x_data) > window:
                self.ax.set_xlim(len(x_data) - window, len(x_data) + 2)
            else:
                self.ax.set_xlim(0, window)
            
            visible_y = y_data[-window:]
            min_y, max_y = min(visible_y) - 5, max(visible_y) + 5
            self.ax.set_ylim(min_y, max_y)
        
        self.canvas_graph.draw_idle()

    def action_start_stream(self):
        """ Извиква се при старт на стрийма. Праща Modbus команда и рисува зелена линия. """
        self.write_register(REG_STREAM_CTRL, 1)
        # Добавяме събитието с текущия брой отчетени стойности
        self.stream_events.append((len(self.cycle_history), "green"))
        self.update_graph()

    def action_stop_stream(self):
        """ Извиква се при стоп на стрийма. Праща Modbus команда и рисува червена линия. """
        self.write_register(REG_STREAM_CTRL, 0)
        self.stream_events.append((len(self.cycle_history), "red"))
        self.update_graph()

    def ip_to_regs(self, ip_str):
        parts = [int(p) for p in ip_str.split('.')]
        return (parts[0] << 8) | parts[1], (parts[2] << 8) | parts[3]

    def regs_to_ip(self, high, low):
        return f"{(high >> 8) & 0xFF}.{high & 0xFF}.{(low >> 8) & 0xFF}.{low & 0xFF}"

    def blink_cycle_led(self):
        self.cycle_led_canvas.itemconfig(self.led_cycle, fill="lime green")
        self.root.after(300, lambda: self.cycle_led_canvas.itemconfig(self.led_cycle, fill="gray30"))

    def blink_enc_led(self):
        self.enc_led_canvas.itemconfig(self.led_enc, fill="lime green")
        self.root.after(100, lambda: self.enc_led_canvas.itemconfig(self.led_enc, fill="gray30"))

    def blink_counter_led(self):
        self.canvas_count.itemconfig(self.led_count, fill="lime green")
        self.root.after(300, lambda: self.canvas_count.itemconfig(self.led_count, fill="gray30"))

    def update_state_led(self, state_idx):
        if state_idx > 0: 
            self.state_led_canvas.itemconfig(self.led_state, fill="lime green")
        else: 
            self.state_led_canvas.itemconfig(self.led_state, fill="gray30")

    def toggle_connection(self):
        if not self.is_connected:
            ip = self.conn_ip_var.get()
            self.client = ModbusTcpClient(ip, port=502)
            if self.client.connect():
                self.is_connected = True
                self.btn_connect.config(text="Disconnect")
                self.enable_controls(True)
                self.load_parameters() 
                self.poll_telemetry() 
            else:
                messagebox.showerror("Error", "Could not connect to STM32.")
        else:
            self.is_connected = False
            if self.poll_job:
                self.root.after_cancel(self.poll_job)
            self.client.close()
            self.btn_connect.config(text="Connect")
            self.enable_controls(False)
            self.enc_led_canvas.itemconfig(self.led_enc, fill="gray30")
            self.state_led_canvas.itemconfig(self.led_state, fill="gray30")
            self.cycle_led_canvas.itemconfig(self.led_cycle, fill="gray30")

    def enable_controls(self, state):
        st = "normal" if state else "disabled"
        self.btn_start_motor.config(state=st)
        self.btn_stop_motor.config(state=st)
        self.btn_start_stream.config(state=st)
        self.btn_stop_stream.config(state=st)
        self.btn_load.config(state=st)
        self.btn_apply.config(state=st)
        self.btn_save.config(state=st)
        self.btn_reboot.config(state=st)

    def load_parameters(self):
        if not self.is_connected: return
        try:
            # <--- NEW: Read 4 registers instead of 3 to include Microsteps --->
            res = self.client.read_holding_registers(address=201, count=4)
            if not res.isError():
                self.motor_dir_var.set(res.registers[0])
                self.rpm_var.set(str(res.registers[1] / 10.0))
                self.accel_var.set(str(res.registers[2] / 10.0))
                self.microsteps_var.set(str(res.registers[3])) # Load microsteps
            
            res = self.client.read_holding_registers(address=301, count=3)
            if not res.isError():
                self.enc_dir_var.set(res.registers[0])
                self.baud_var.set(str(res.registers[1] * 100))
                self.auto_stream_var.set(res.registers[2])
            res = self.client.read_holding_registers(address=310, count=6)
            if not res.isError():
                self.cfg_ip_var.set(self.regs_to_ip(res.registers[0], res.registers[1]))
                self.cfg_mask_var.set(self.regs_to_ip(res.registers[2], res.registers[3]))
                self.cfg_gw_var.set(self.regs_to_ip(res.registers[4], res.registers[5]))
        except Exception: pass

    def apply_parameters(self):
        if not self.is_connected: return
        try:
            rpm = int(float(self.rpm_var.get()) * 10)
            accel = int(float(self.accel_var.get()) * 10)
            baud = int(int(self.baud_var.get()) / 100)
            microsteps = int(self.microsteps_var.get()) # Parse microsteps
            
            self.client.write_register(address=REG_MOTOR_DIR, value=self.motor_dir_var.get())
            self.client.write_register(address=REG_MOTOR_RPM, value=rpm)
            self.client.write_register(address=REG_MOTOR_ACCEL, value=accel)
            self.client.write_register(address=REG_MOTOR_MICROSTEPS, value=microsteps) # Send microsteps
            
            self.client.write_register(address=REG_ENCODER_DIR, value=self.enc_dir_var.get())
            self.client.write_register(address=REG_UART_BAUD, value=baud)
            self.client.write_register(address=REG_AUTO_STREAM, value=self.auto_stream_var.get())
            
            ip_h, ip_l = self.ip_to_regs(self.cfg_ip_var.get())
            self.client.write_register(address=REG_IP_HIGH, value=ip_h)
            self.client.write_register(address=REG_IP_LOW, value=ip_l)
            messagebox.showinfo("Success", "Applied to RAM!")
        except Exception as e: messagebox.showerror("Error", str(e))

    def save_to_flash(self):
        if not self.is_connected: return
        if messagebox.askyesno("Save", "Burn to STM32 Flash?"):
            try: self.client.write_register(address=REG_SAVE_SETTINGS, value=1)
            except Exception: pass

    def reboot_board(self):
        if not self.is_connected: return
        if messagebox.askyesno("Reboot", "Reboot board?"):
            try: self.client.write_register(address=REG_SYSTEM_REBOOT, value=1); self.toggle_connection()
            except Exception: pass

    def write_register(self, reg_addr, value):
        if self.is_connected:
            try: self.client.write_register(address=reg_addr, value=value)
            except Exception: pass

    def poll_telemetry(self):
        if self.is_connected:
            try:
                # Read 4 registers starting from REG_ENCODER_POS to include the new cycle counter
                response = self.client.read_holding_registers(address=REG_ENCODER_POS, count=4)
                
                if not response.isError():
                    enc_pos = response.registers[0]
                    state_idx = response.registers[1]
                    cycle_time = response.registers[2]
                    cycle_counter = response.registers[3]  # <--- Extract the heartbeat counter

                    # Update GUI labels with real-time data
                    self.lbl_enc_pos.config(text=str(enc_pos))
                    self.lbl_motor_state.config(text=MOTOR_STATES[state_idx] if state_idx < len(MOTOR_STATES) else "ERROR")
                    self.lbl_cycle_time.config(text=f"{cycle_time} ms")
                    self.lbl_cycle_count.config(text=str(cycle_counter))
                    
                    # Update the visual motor state LED indicator
                    self.update_state_led(state_idx)
                    
                    # Check if encoder position has changed to blink the position LED
                    if enc_pos != self.last_enc_pos and self.last_enc_pos != -1: 
                        self.blink_enc_led()
                        
                    # IMPORTANT: Detect a new full rotation by checking cycle_counter instead of cycle_time!
                    # This ensures the graph updates properly even if the motor speed is perfectly constant.
                    if cycle_counter != getattr(self, 'last_cycle_counter', -1) and cycle_counter != 0:
                        self.blink_cycle_led()
                        self.blink_counter_led()
                        self.cycle_history.append(cycle_time)
                        self.update_graph()
                        
                    # Store current values for comparison in the next polling cycle
                    self.last_enc_pos = enc_pos
                    self.last_cycle_time = cycle_time
                    self.last_cycle_counter = cycle_counter  # <--- Store the latest counter state
            
            except Exception: 
                pass # Silently ignore read errors to prevent GUI crashes during brief timeouts
            
        # Schedule the next polling cycle in 500ms
        self.poll_job = self.root.after(500, self.poll_telemetry)

if __name__ == "__main__":
    root = tk.Tk()
    app = MotorControlApp(root)
    root.mainloop()