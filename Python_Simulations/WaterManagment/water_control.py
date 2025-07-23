import tkinter as tk
from tkinter import ttk
import time

# --- Decision Logic ---

class WaterSystem:
    def __init__(self):
        # System states
        self.current_case = "RST"
        self.tank_states = {"CE": False, "DF": False, "DE": True, "RF": False, "RE": True}
        self.last_action_time = time.time()
        self.timeout_duration = 300  # 5 minutes in seconds
        self.transitioning = False
        self.transition_start = None
        self.transition_target_case = None
        self.transition_button = None

        # Case configurations: {case: {valves: {A, B, C, D, E}, pumps: {PE, PD, PV, PP}}}
        self.case_configs = {
            "RST": {"valves": {"A": 0, "B": 0, "C": 0, "D": 0, "E": 0}, "pumps": {"PE": 0, "PD": 0, "PV": 0, "PP": 0}},
            "E1":  {"valves": {"A": 0, "B": 1, "C": 1, "D": 0, "E": 0}, "pumps": {"PE": 1, "PD": 0, "PV": 0, "PP": 0}},
            "E2":  {"valves": {"A": 0, "B": 1, "C": 0, "D": 0, "E": 0}, "pumps": {"PE": 1, "PD": 0, "PV": 0, "PP": 0}},
            "E3":  {"valves": {"A": 1, "B": 1, "C": 1, "D": 0, "E": 0}, "pumps": {"PE": 1, "PD": 0, "PV": 0, "PP": 0}},
            "E4":  {"valves": {"A": 1, "B": 1, "C": 0, "D": 0, "E": 0}, "pumps": {"PE": 1, "PD": 0, "PV": 0, "PP": 0}},
            "D1":  {"valves": {"A": 0, "B": 0, "C": 1, "D": 1, "E": 0}, "pumps": {"PE": 1, "PD": 1, "PV": 0, "PP": 0}},
            "D2":  {"valves": {"A": 0, "B": 0, "C": 0, "D": 1, "E": 0}, "pumps": {"PE": 1, "PD": 1, "PV": 0, "PP": 0}},
            "D3":  {"valves": {"A": 1, "B": 0, "C": 1, "D": 1, "E": 0}, "pumps": {"PE": 1, "PD": 1, "PV": 0, "PP": 0}},
            "D4":  {"valves": {"A": 1, "B": 0, "C": 0, "D": 1, "E": 0}, "pumps": {"PE": 1, "PD": 1, "PV": 0, "PP": 0}},
            "V1":  {"valves": {"A": 0, "B": 0, "C": 1, "D": 0, "E": 1}, "pumps": {"PE": 0, "PD": 0, "PV": 1, "PP": 0}},
            "V2":  {"valves": {"A": 0, "B": 0, "C": 0, "D": 0, "E": 1}, "pumps": {"PE": 0, "PD": 0, "PV": 1, "PP": 0}},
            "P1":  {"valves": {"A": 0, "B": 0, "C": 0, "D": 0, "E": 1}, "pumps": {"PE": 0, "PD": 0, "PV": 0, "PP": 1}},
        }

        # Button colors for physical buttons
        self.physical_button_colors = {
            "E1":  {"BE1": "Green", "BE2": "Red", "BD1": "Off", "BD2": "Off"},
            "E2":  {"BE1": "Green", "BE2": "Green", "BD1": "Off", "BD2": "Off"},
            "E3":  {"BE1": "Red", "BE2": "Red", "BD1": "Off", "BD2": "Off"},
            "E4":  {"BE1": "Red", "BE2": "Green", "BD1": "Off", "BD2": "Off"},
            "D1":  {"BE1": "Off", "BE2": "Off", "BD1": "Green", "BD2": "Red"},
            "D2":  {"BE1": "Off", "BE2": "Off", "BD1": "Green", "BD2": "Green"},
            "D3":  {"BE1": "Off", "BE2": "Off", "BD1": "Red", "BD2": "Red"},
            "D4":  {"BE1": "Off", "BE2": "Off", "BD1": "Red", "BD2": "Green"},
            "V1":  {"BE1": "Off", "BE2": "Off", "BD1": "Off", "BD2": "Off"},
            "V2":  {"BE1": "Off", "BE2": "Off", "BD1": "Off", "BD2": "Off"},
            "P1":  {"BE1": "Off", "BE2": "Off", "BD1": "Off", "BD2": "Off"},
            "RST": {"BE1": "Off", "BE2": "Off", "BD1": "Off", "BD2": "Off"},
        }

        # Digital button states
        self.digital_button_states = {
            "E1":  {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "Off"},
            "E2":  {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "Off"},
            "E3":  {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "Off"},
            "E4":  {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "Off"},
            "D1":  {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "Off"},
            "D2":  {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "Off"},
            "D3":  {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "Off"},
            "D4":  {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "Off"},
            "V1":  {"BV1": "On", "BV2": "Off", "BP1": "Off", "BRST": "Off"},
            "V2":  {"BV1": "Off", "BV2": "On", "BP1": "Off", "BRST": "Off"},
            "P1":  {"BV1": "Off", "BV2": "Off", "BP1": "On", "BRST": "Off"},
            "RST": {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "On"},
        }

        # Incompatible cases
        self.incompatible_cases = {
            "CE": ["E1", "E2", "D1", "D2"],
            "DF": ["E1", "E3", "D1", "D3"],
            "DE": ["V1"],
            "RF": ["E2", "E3", "E4", "D2", "D3", "D4", "P1"],
            "RE": ["E3", "E4", "D3", "D4", "V2"],
        }

        # Button click to target case mapping
        self.button_targets = {
            "BE1_SC": {
                "RST": "E1",
                "E1": "E3",
                "E2": "E4",
                "E3": "E1",
                "E4": "E2", 
                "D1": "E1",
                "D2": "E1",
                "D3": "E1",
                "D4": "E1",
                "V1": "E1",
                "V2": "E1",
                "P1": "E1",
            },
            "BE2_SC": {
                "E1": "E2",
                "E2": "E1",
                "E3": "E4",
                "E4": "E3",
            },
            "BD1_SC": {
                "RST": "D1",
                "E1": "D1",
                "E2": "D1",
                "E3": "D1",
                "E4": "D1",
                "D1": "D3",
                "D2": "D4",  # Fixed: D2 -> D4 (keep BD2=Green)
                "D3": "D1",
                "D4": "D2",  
                "V1": "D1",
                "V2": "D1",
                "P1": "D1",
            },
            "BD2_SC": {
                "D1": "D2",
                "D2": "D1",
                "D3": "D4",
                "D4": "D3",
            },
            "BV1_SC": {"any": "V1"},
            "BV2_SC": {"any": "V2"},
            "BP1_SC": {"any": "P1"},
            "BRST_SC": {"any": "RST"},
            "BE1_LC": {"any": "RST"},
            "BD1_LC": {"any": "RST"},
            "BE2_LC": {"any": None},  # Ignored
            "BD2_LC": {"any": None},  # Ignored
            "BV1_LC": {"any": None},  # Ignored
            "BV2_LC": {"any": None},  # Ignored
            "BP1_LC": {"any": None},  # Ignored
        }

    def is_case_incompatible(self, case):
        """Check if a case is incompatible with current tank states."""
        for state, incompatible_cases in self.incompatible_cases.items():
            if self.tank_states[state] and case in incompatible_cases:
                return state
        return None

    def determine_target_case(self, button, click_type):
        """Determine the target case based on button click."""
        key = f"{button}_{click_type}"
        if key in self.button_targets:
            target = self.button_targets[key]
            if "any" in target:
                return target["any"]
            return target.get(self.current_case, self.current_case)
        return self.current_case

    def start_transition(self, button, target_case):
        """Start a 2-second transition to a new case."""
        self.transitioning = True
        self.transition_start = time.time()
        self.transition_target_case = target_case
        self.transition_button = button

    def end_transition(self):
        """End the transition, checking compatibility and updating state."""
        incompatible_state = self.is_case_incompatible(self.transition_target_case)
        self.transitioning = False
        target_case = self.transition_target_case
        self.transition_target_case = None
        self.transition_button = None
        if incompatible_state:
            # Check if current case is still compatible
            current_incompatible = self.is_case_incompatible(self.current_case)
            if current_incompatible:
                self.current_case = "RST"
                self.last_action_time = time.time()
                return self.current_case, f"Cannot change to {target_case}: {incompatible_state}=True, resetting to RST", "red"
            return self.current_case, f"Cannot change to {target_case}: {incompatible_state}=True", "red"
        self.current_case = target_case
        self.last_action_time = time.time()
        return self.current_case, f"Changed to {self.current_case}", "green"

    def update_system(self, button=None, click_type=None):
        """Update the system state based on button click or timeout."""
        # Check if in transition
        if self.transitioning:
            if time.time() - self.transition_start >= 0.2:  # 2-second transition complete
                return self.end_transition()
            return self.current_case, None, None

        # Check for timeout (5 minutes inactivity)
        if time.time() - self.last_action_time > self.timeout_duration:
            self.start_transition(None, "RST")
            return self.current_case, "Timeout triggered, transitioning to RST", "yellow"

        # Handle button click
        if button and click_type:
            target_case = self.determine_target_case(button, click_type)
            if target_case is None:  # Invalid click (e.g., BE2_LC)
                return self.current_case, f"Ignored {button} {click_type}", "red"
            incompatible_state = self.is_case_incompatible(target_case)
            if incompatible_state:
                return self.current_case, f"Cannot change to {target_case}: {incompatible_state}=True", "red"
            if target_case != self.current_case:
                self.start_transition(button, target_case)
                return self.current_case, f"Transitioning to {target_case}", "yellow"
            return self.current_case, None, None

        # No button click, check if current case is still compatible
        incompatible_state = self.is_case_incompatible(self.current_case)
        if incompatible_state:
            self.start_transition(None, "RST")
            return self.current_case, f"Current case {self.current_case} incompatible: {incompatible_state}=True, transitioning to RST", "red"
        return self.current_case, None, None

    def get_button_colors(self):
        """Return physical button colors, applying yellow during transition."""
        if self.transitioning and self.transition_button in ["BE1", "BE2", "BD1", "BD2"]:
            colors = self.physical_button_colors.get(self.current_case, {"BE1": "Off", "BE2": "Off", "BD1": "Off", "BD2": "Off"}).copy()
            if self.transition_target_case.startswith("E") and self.transition_button in ["BE1", "BE2"]:
                colors["BE1"] = "Yellow"
                colors["BE2"] = "Yellow"
            elif self.transition_target_case.startswith("D") and self.transition_button in ["BD1", "BD2"]:
                colors["BD1"] = "Yellow"
                colors["BD2"] = "Yellow"
            elif self.transition_target_case == "RST" and self.transition_button in ["BE1", "BD1"]:
                colors[self.transition_button] = "Yellow"
            return colors
        return self.physical_button_colors.get(self.current_case, {"BE1": "Off", "BE2": "Off", "BD1": "Off", "BD2": "Off"})

    def get_digital_button_states(self):
        """Return digital button states based on current case."""
        return self.digital_button_states.get(self.current_case, {"BV1": "Off", "BV2": "Off", "BP1": "Off", "BRST": "On"})

    def get_valve_pump_states(self):
        """Return valve and pump states based on current case."""
        return self.case_configs.get(self.current_case, {"valves": {"A": 0, "B": 0, "C": 0, "D": 0, "E": 0}, "pumps": {"PE": 0, "PD": 0, "PV": 0, "PP": 0}})

    def get_transition_status(self):
        """Return transition status for UI."""
        return self.transitioning, self.transition_button, self.transition_target_case

    def update_tank_states(self, clean_level, dirty_level, recycled_level):
        """Update tank states based on slider levels (0-100)."""
        self.tank_states["CE"] = clean_level <= 0
        self.tank_states["DF"] = dirty_level >= 100
        self.tank_states["DE"] = dirty_level <= 0
        self.tank_states["RF"] = recycled_level >= 100
        self.tank_states["RE"] = recycled_level <= 0

# --- UI Implementation ---

class WaterSystemUI:
    def __init__(self, root):
        self.system = WaterSystem()
        self.root = root
        self.root.title("Water System Control")
        self.blinking = False
        self.blink_state = False
        self.blink_button = None
        self.blink_color = None

        # Frames
        self.button_frame = ttk.LabelFrame(root, text="Physical Buttons", padding=10)
        self.button_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        self.digital_frame = ttk.LabelFrame(root, text="Digital Buttons", padding=10)
        self.digital_frame.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")
        self.tank_frame = ttk.LabelFrame(root, text="Tank Levels", padding=10)
        self.tank_frame.grid(row=1, column=0, padx=10, pady=10, sticky="nsew")
        self.state_frame = ttk.LabelFrame(root, text="System State", padding=10)
        self.state_frame.grid(row=1, column=1, padx=10, pady=10, sticky="nsew")
        self.log_frame = ttk.LabelFrame(root, text="Log", padding=10)
        self.log_frame.grid(row=2, column=0, columnspan=2, padx=10, pady=10, sticky="nsew")

        # Physical Buttons
        self.physical_buttons = {}
        for idx, btn in enumerate(["BE1", "BE2", "BD1", "BD2"]):
            frame = ttk.Frame(self.button_frame)
            frame.grid(row=idx, column=0, pady=5)
            ttk.Label(frame, text=f"{btn}:").grid(row=0, column=0)
            color_label = ttk.Label(frame, text="Off", width=10, anchor="center", relief="sunken")
            color_label.grid(row=0, column=1, padx=5)
            ttk.Button(frame, text="Short Click", command=lambda b=btn: self.handle_button_click(b, "SC")).grid(row=0, column=2, padx=5)
            ttk.Button(frame, text="Long Click", command=lambda b=btn: self.handle_button_click(b, "LC")).grid(row=0, column=3, padx=5)
            self.physical_buttons[btn] = color_label

        # Digital Buttons
        self.digital_buttons = {}
        for idx, btn in enumerate(["BV1", "BV2", "BP1", "BRST"]):
            frame = ttk.Frame(self.digital_frame)
            frame.grid(row=idx, column=0, pady=5)
            ttk.Label(frame, text=f"{btn}:").grid(row=0, column=0)
            state_label = ttk.Label(frame, text="Off", width=10, anchor="center", relief="sunken")
            state_label.grid(row=0, column=1, padx=5)
            ttk.Button(frame, text="Short Click", command=lambda b=btn: self.handle_button_click(b, "SC")).grid(row=0, column=2, padx=5)
            ttk.Button(frame, text="Long Click", command=lambda b=btn: self.handle_button_click(b, "LC")).grid(row=0, column=3, padx=5)
            self.digital_buttons[btn] = state_label

        # Tank Sliders
        self.tank_sliders = {}
        for idx, tank in enumerate(["Clean (EP)", "Dirty (ES)", "Recycled (ER)"]):
            frame = ttk.Frame(self.tank_frame)
            frame.grid(row=idx, column=0, pady=5)
            ttk.Label(frame, text=f"{tank}:").grid(row=0, column=0)
            slider = ttk.Scale(frame, from_=0, to=100, orient="horizontal", length=200, command=self.update_tanks)
            slider.grid(row=0, column=1, padx=5)
            value_label = ttk.Label(frame, text="0%", width=10)
            value_label.grid(row=0, column=2)
            self.tank_sliders[tank] = (slider, value_label)
            # Set initial slider values and update labels
            initial_value = 50 if tank == "Recycled (ER)" else 0
            slider.set(initial_value)
            value_label.config(text=f"{int(initial_value)}%")

        # System State Display
        self.state_label = ttk.Label(self.state_frame, text="Current Case: RST")
        self.state_label.grid(row=0, column=0, pady=5)
        self.valve_labels = {}
        for idx, valve in enumerate(["A", "B", "C", "D", "E"]):
            label = ttk.Label(self.state_frame, text=f"Valve {valve}: 0")
            label.grid(row=idx+1, column=0, pady=2)
            self.valve_labels[valve] = label
        self.pump_labels = {}
        for idx, pump in enumerate(["PE", "PD", "PV", "PP"]):
            label = ttk.Label(self.state_frame, text=f"Pump {pump}: 0")
            label.grid(row=idx+6, column=0, pady=2)
            self.pump_labels[pump] = label

        # Log Display
        self.log_text = tk.Text(self.log_frame, height=5, width=50, wrap="word")
        self.log_text.grid(row=0, column=0, padx=5, pady=5)
        scrollbar = ttk.Scrollbar(self.log_frame, orient="vertical", command=self.log_text.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.log_text.config(yscrollcommand=scrollbar.set)
        self.log_text.tag_configure("green", foreground="green")
        self.log_text.tag_configure("red", foreground="red")
        self.log_text.tag_configure("yellow", foreground="yellow")
        self.log_text.config(state="disabled")

        # Start periodic update
        self.update_ui()

    def log_message(self, message, color):
        """Add a message to the log with specified color."""
        self.log_text.config(state="normal")
        timestamp = time.strftime("%H:%M:%S")
        self.log_text.insert("end", f"[{timestamp}] {message}\n", color)
        self.log_text.see("end")
        self.log_text.config(state="disabled")

    def handle_button_click(self, button, click_type):
        """Handle button click and update system."""
        if self.system.transitioning:
            self.log_message(f"Ignored {button} {click_type}: System is transitioning", "red")
            return
        new_case, message, color = self.system.update_system(button, click_type)
        target_case = self.system.transition_target_case if self.system.transitioning else new_case
        if message:
            self.log_message(message, color)
        if color == "red":  # Incompatible case
            self.start_blinking(button, self.get_target_color(button, target_case))
        self.update_ui()

    def get_target_color(self, button, target_case):
        """Determine the target color for a button based on the target case."""
        if button in ["BE1", "BE2", "BD1", "BD2"]:
            return self.system.physical_button_colors.get(target_case, {}).get(button, "Off")
        return "Off"

    def start_blinking(self, button, color):
        """Start blinking effect for incompatible case."""
        self.blinking = True
        self.blink_button = button
        self.blink_color = color
        self.blink_state = False

    def stop_blinking(self):
        """Stop blinking effect."""
        self.blinking = False
        self.blink_button = None
        self.blink_color = None
        

    def update_tanks(self, *args):
        """Update tank states based on slider values."""
        clean_level = self.tank_sliders["Clean (EP)"][0].get()
        dirty_level = self.tank_sliders["Dirty (ES)"][0].get()
        recycled_level = self.tank_sliders["Recycled (ER)"][0].get()
        self.system.update_tank_states(clean_level, dirty_level, recycled_level)
        # Update percentage labels
        self.tank_sliders["Clean (EP)"][1].config(text=f"{int(clean_level)}%")
        self.tank_sliders["Dirty (ES)"][1].config(text=f"{int(dirty_level)}%")
        self.tank_sliders["Recycled (ER)"][1].config(text=f"{int(recycled_level)}%")
        new_case, message, color = self.system.update_system()
        if message:
            self.log_message(message, color)
        self.update_ui()

    def update_ui(self):
        """Update UI elements based on system state."""
        # Update system state
        new_case, message, color = self.system.update_system()
        if message:
            self.log_message(message, color)

        # Stop blinking if the error is resolved
        if not self.system.is_case_incompatible(new_case):
            self.stop_blinking()

        # Update button colors
        colors = self.system.get_button_colors()
        for btn, color in colors.items():
            bg_color = {"Green": "green", "Red": "red", "Off": "grey", "Yellow": "yellow"}.get(color, "grey")
            if self.blinking and btn == self.blink_button and not self.system.transitioning:
                bg_color = {"Green": "green", "Red": "red", "Off": "grey"}.get(self.blink_color, "grey") if self.blink_state else "grey"
                self.blink_state = not self.blink_state
            else:
                # Ensure the button color is updated correctly when not blinking
                bg_color = {"Green": "green", "Red": "red", "Off": "grey", "Yellow": "yellow"}.get(color, "grey")
            self.physical_buttons[btn].config(text=color, background=bg_color)

        # Update digital button states
        states = self.system.get_digital_button_states()
        for btn, state in states.items():
            bg_color = "lightgreen" if state == "On" else "grey"
            self.digital_buttons[btn].config(text=state, background=bg_color)

        # Update system state display
        self.state_label.config(text=f"Current Case: {self.system.current_case}")
        valve_pump_states = self.system.get_valve_pump_states()
        for valve, state in valve_pump_states["valves"].items():
            self.valve_labels[valve].config(text=f"Valve {valve}: {state}")
        for pump, state in valve_pump_states["pumps"].items():
            self.pump_labels[pump].config(text=f"Pump {pump}: {state}")

        # Schedule next update
        update_interval = 500 if (self.blinking or self.system.transitioning) else 1000
        self.root.after(update_interval, self.update_ui)

def main():
    root = tk.Tk()
    app = WaterSystemUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()