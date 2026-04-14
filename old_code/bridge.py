import serial
import json
import time
import httpx
import threading
import sys
from datetime import datetime
from supabase import create_client, Client

# ============ CONFIGURATION ============
SUPABASE_URL = "https://vpnrgikmczryyryjtwai.supabase.co"
SUPABASE_KEY = "sb_publishable_3zeX8QdVpEYZitcA8dY_kQ_t48x_krs"
TELEGRAM_BOT_TOKEN = "8690059316:AAFgC6wj0lFZEPh-M_NRoVtF6H3hXnLe8bw"
TELEGRAM_CHAT_ID = "7889066155"

# Serial Port - Change this to your ESP32 port (e.g., 'COM3' or '/dev/ttyUSB0')
SERIAL_PORT = 'COM3' 
BAUD_RATE = 115200

# ============ UTILS ============
def log(msg):
    """Print with precise timestamp for diagnostics"""
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{timestamp}] {msg}")

# Initialize HTTP Client for Telegram (Persistent Connection)
http_client = httpx.Client(timeout=10.0)

# ============ INITIALIZATION ============
log("Initializing Bridge...")
supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)

def send_telegram(message):
    log(f"Sending Telegram notification...")
    url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
    payload = {
        "chat_id": TELEGRAM_CHAT_ID,
        "text": message,
        "parse_mode": "Markdown"
    }
    try:
        start_t = time.time()
        response = http_client.post(url, json=payload)
        elapsed = (time.time() - start_t) * 1000
        log(f"Telegram finished in {elapsed:.0f}ms (Status: {response.status_code})")
        return response.status_code == 200
    except Exception as e:
        log(f"Telegram Error: {e}")
        return False

def format_currency(value):
    return f"{value:,}".replace(",", ".")

def handle_save_transaction(data):
    nominal = data.get("nominal", 0)
    total = data.get("total", 0)
    
    log(f"Processing Transaction: Rp {nominal}")
    
    # 1. Save to Supabase with timer
    try:
        start_t = time.time()
        supabase.table("transactions").insert({
            "nominal": nominal,
            "total_balance": total
        }).execute()
        elapsed = (time.time() - start_t) * 1000
        log(f"Supabase transaction saved in {elapsed:.0f}ms")
    except Exception as e:
        log(f"Supabase Error: {e}")
        return {"status": "error", "msg": "Supabase failed"}

    # 2. Send Telegram
    msg = (
        "💰 *ADA UANG MASUK!*\n\n"
        "━━━━━━━━━━━━━━━\n"
        f"💵 *Nominal:* \n      Rp {format_currency(nominal)}\n\n"
        f"📊 *Total Tabungan:* \n      Rp {format_currency(total)}\n\n"
        f"⏰ *Waktu:* \n      {datetime.now().strftime('%H:%M %d/%m/%Y')}\n"
        "━━━━━━━━━━━━━━━"
    )
    send_telegram(msg)
    
    return {"status": "ok", "msg": "Success"}

def handle_get_balance():
    log("Fetching Balance...")
    try:
        start_t = time.time()
        response = supabase.table("transactions")\
            .select("total_balance")\
            .order("id", desc=True)\
            .limit(1)\
            .execute()
        elapsed = (time.time() - start_t) * 1000
        
        balance = 0
        if response.data:
            balance = response.data[0]["total_balance"]
        
        log(f"Balance fetched in {elapsed:.0f}ms (Total: Rp {format_currency(balance)})")
        return {"status": "ok", "balance": balance}
    except Exception as e:
        log(f"Error fetching balance: {e}")
        return {"status": "error", "balance": 0}

def handle_get_calibrations():
    log("Fetching Calibrations...")
    try:
        start_t = time.time()
        response = supabase.table("calibrations").select("*").execute()
        elapsed = (time.time() - start_t) * 1000
        log(f"Calibrations fetched in {elapsed:.0f}ms ({len(response.data)} items)")
        return {"status": "ok", "data": response.data}
    except Exception as e:
        log(f"Error fetching calibrations: {e}")
        return {"status": "error", "data": []}

def handle_save_calibration(data):
    log(f"Saving Calibration for Rp {data.get('nominal')}")
    try:
        start_t = time.time()
        supabase.table("calibrations").insert({
            "nominal": data.get("nominal"),
            "ref_r": data.get("ref_r"),
            "ref_g": data.get("ref_g"),
            "ref_b": data.get("ref_b", 0),
            "tolerance": data.get("tolerance")
        }).execute()
        elapsed = (time.time() - start_t) * 1000
        log(f"Calibration saved in {elapsed:.0f}ms")
        return {"status": "ok"}
    except Exception as e:
        log(f"Error saving calibration: {e}")
        return {"status": "error"}

def keyboard_input_handler(ser):
    log("Serial Bridge: Keyboard Relay Active")
    log("Type commands like /cal, /list, /clear here.")
    while True:
        try:
            # Read from terminal
            cmd = sys.stdin.readline().strip()
            if cmd:
                # Send to ESP32
                ser.write((cmd + "\n").encode('utf-8'))
                log(f"[PC -> ESP32]: {cmd}")
        except Exception as e:
            log(f"Keyboard Error: {e}")
            break

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        ser.reset_input_buffer()  # Clear garbage data on startup
        log(f"Connected to {SERIAL_PORT} at {BAUD_RATE}")
        
        # Start keyboard thread as daemon (will exit when main loop exits)
        input_thread = threading.Thread(target=keyboard_input_handler, args=(ser,), daemon=True)
        input_thread.start()
        
        while True:
            if ser.in_waiting > 0:
                # Use errors='ignore' to handle non-utf8 noise during boot
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line: continue
                
                # Check if it's a JSON command (starts with {)
                if line.startswith('{'):
                    try:
                        request = json.loads(line)
                        cmd = request.get("cmd")
                        response = {"status": "unknown"}
                        
                        if cmd == "save_tr":
                            # 1. SEND FAST RESPONSE TO ESP32 FIRST
                            ser.write(b'{"status": "ok", "msg": "Processing"}\n')
                            log("Bridge: Quick response sent to ESP32 (Backgrounding cloud tasks)")
                            
                            # 2. RUN HEAVY TASKS IN BACKGROUND
                            threading.Thread(target=handle_save_transaction, args=(request,), daemon=True).start()
                            continue

                        elif cmd == "get_balance":
                            response = handle_get_balance()
                        elif cmd == "get_cal":
                            response = handle_get_calibrations()
                        elif cmd == "save_cal":
                            # Fast response for calibration too
                            ser.write(b'{"status": "ok"}\n')
                            threading.Thread(target=handle_save_calibration, args=(request,), daemon=True).start()
                            continue
                        
                        # Send response back to ESP32 for synchronous commands (get_*)
                        resp_json = json.dumps(response)
                        ser.write((resp_json + "\n").encode('utf-8'))
                        log(f"Bridge response ready for ESP32")
                        
                    except json.JSONDecodeError:
                        log(f"Invalid JSON: {line}")
                else:
                    # Just print normal debug messages from ESP32
                    log(f"ESP32: {line}")
            
            time.sleep(0.01)
            
    except serial.SerialException as e:
        print(f"Serial Error: {e}")
        print("Retrying in 5 seconds...")
        time.sleep(5)
        main()
    except KeyboardInterrupt:
        print("Bridge stopped.")

if __name__ == "__main__":
    main()
