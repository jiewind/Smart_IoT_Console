import tkinter as tk
from tkinter import font
import socket
import threading
import json
import time
import csv
import os

# ==========================================
# 💾 全局变量与配置
# ==========================================
client_conn = None
last_receive_time = time.time()  # 记录最后一次收到心跳/数据的时间
is_device_online = False         # 设备在线状态标识
CSV_FILE_PATH = "telemetry_data.csv" # 历史数据持久化文件

# 历史数据缓存（用于绘制最近30次的数据折线图）
temp_history = []
max_history_len = 30

# 初始化 CSV 文件头
if not os.path.exists(CSV_FILE_PATH):
    with open(CSV_FILE_PATH, mode='w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(["Timestamp", "Temperature", "Humidity", "Light", "LED_Status"])

# ==========================================
# 📡 后台网络线程（守护单片机连接）
# ==========================================
def tcp_server_thread():
    global client_conn, last_receive_time
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(('0.0.0.0', 8080))
    server.listen(1)
    
    update_log("📡 监听端口 8080，等待单片机接入...")
    
    while True:
        conn, addr = server.accept()
        client_conn = conn
        last_receive_time = time.time() # 连入时刷新时间
        update_log(f"✅ 单片机已连入! IP: {addr[0]}")
        
        buffer = ""
        while True:
            try:
                data = conn.recv(1024)
                if not data:
                    break 
                
                # 只要收到任何字节，立刻刷新心跳时间
                last_receive_time = time.time()
                
                buffer += data.decode('utf-8')
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    if line.startswith('{') and line.endswith('}'):
                        try:
                            parsed_data = json.loads(line)
                            # 跨线程安全：调度到主线程执行数据处理与 UI 刷新
                            window.after(0, process_incoming_data, parsed_data)
                        except json.JSONDecodeError:
                            pass
            except Exception:
                break
        
        client_conn = None
        window.after(0, set_device_offline)
        update_log("❌ 单片机连接断开，正在重新等待...")

# ==========================================
# 🧠 数据处理中枢：数据解析、持久化、图表缓存
# ==========================================
def process_incoming_data(data):
    global is_device_online, temp_history
    
    # 1. 改变在线状态指示灯
    if not is_device_online:
        is_device_online = True
        lbl_status.config(text="● 在线 (ONLINE)", fg="#2ecc71")
    
    # 2. 更新大屏看板数字
    t = data.get('temp', 0)
    h = data.get('humi', 0)
    l = data.get('light', 0)
    led = data.get('led', 'OFF')
    page = data.get('page', 1)
    
    lbl_temp.config(text=f"{t} °C")
    lbl_humi.config(text=f"{h} %")
    lbl_light.config(text=f"{l} %")
    lbl_led.config(text=f"{led}")
    lbl_page.config(text=f"Page {page}")
    
    # 3. 【功能一：数据持久化】自动写入本地 CSV 文件
    current_time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    try:
        with open(CSV_FILE_PATH, mode='a', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow([current_time_str, t, h, l, led])
    except Exception as e:
        update_log(f"⚠️ CSV 写入失败: {str(e)}")
        
    # 4. 【功能二：图表历史缓存】
    temp_history.append(t)
    if len(temp_history) > max_history_len:
        temp_history.pop(0) # 保持最大 30 个历史数据点
        
    # 5. 触发图表重绘
    draw_historical_chart()

# ==========================================
# 💓 心跳超时监测雷达（每 1 秒在主线程触发一次）
# ==========================================
def check_heartbeat_radar():
    global is_device_online
    # 如果当前时间距离最后一次收到数据超过 6 秒（3个单片机周期没发数）
    if time.time() - last_receive_time > 6.0:
        if is_device_online:
            set_device_offline()
    # 递归调用，1000ms 后再次查岗
    window.after(1000, check_heartbeat_radar)

def set_device_offline():
    global is_device_online, temp_history
    is_device_online = False
    lbl_status.config(text="● 离线 (OFFLINE)", fg="#e74c3c")
    lbl_temp.config(text="-- °C")
    lbl_humi.config(text="-- %")
    lbl_light.config(text="-- %")
    lbl_led.config(text="--")
    lbl_page.config(text="Page --")
    temp_history.clear() # 离线后清空当前实时曲线
    draw_historical_chart()
    update_log("🚨 警告：单片机心跳丢失，已判定离线！")

# ==========================================
# 📈 历史图表绘制逻辑（升级版：带坐标系与动态标签）
# ==========================================
def draw_historical_chart():
    canvas.delete("plot") # 擦除老线条和文字
    
    if not temp_history or len(temp_history) < 2:
        canvas.create_text(220, 50, text="等待历史数据积攒...", fill="#7f8c8d", tags="plot")
        return
        
    c_width = 440
    c_height = 100
    
    # 定义图表的内边距 (留出空间画文字)
    pad_left = 30
    pad_right = 15
    pad_top = 15
    pad_bottom = 10
    
    plot_width = c_width - pad_left - pad_right
    plot_height = c_height - pad_top - pad_bottom
    
    # X轴步长与Y轴极值映射 (假设温度在 0~50°C 之间)
    x_step = plot_width / (max_history_len - 1)
    min_val, max_val = 0, 50 
    
    # 1. 绘制 Y 轴背景网格与刻度文字 (每 10 度画一根虚线)
    for v in range(min_val, max_val + 1, 10):
        y = pad_top + plot_height - ((v - min_val) / (max_val - min_val) * plot_height)
        # 画横向深色虚线
        canvas.create_line(pad_left, y, c_width - pad_right, y, fill="#2c3e50", dash=(2, 4), tags="plot")
        # 画左侧温度刻度文字
        canvas.create_text(pad_left - 5, y, text=str(v), fill="#7f8c8d", font=("Consolas", 8), anchor="e", tags="plot")

    # 2. 计算所有点的精准坐标
    points = []
    for i, val in enumerate(temp_history):
        x = pad_left + i * x_step
        y = pad_top + plot_height - ((val - min_val) / (max_val - min_val) * plot_height)
        points.append((x, y))
        
    # 3. 连点成线
    for i in range(len(points) - 1):
        canvas.create_line(points[i][0], points[i][1], points[i+1][0], points[i+1][1], 
                           fill="#3498db", width=2, tags="plot")
                           
    # 4. 高亮最新数据的“动态追踪点”
    last_x, last_y = points[-1]
    last_val = temp_history[-1]
    # 画一个醒目的小红点
    canvas.create_oval(last_x - 3, last_y - 3, last_x + 3, last_y + 3, fill="#e74c3c", outline="", tags="plot")
    # 在红点上方悬浮显示当前精准温度值
    canvas.create_text(last_x, last_y - 12, text=f"{last_val}°C", fill="#e74c3c", font=("Consolas", 9, "bold"), tags="plot")

# ==========================================
# 🎮 控制命令下发
# ==========================================
def send_command(cmd):
    if client_conn:
        try:
            client_conn.sendall((cmd + '\n').encode('utf-8'))
            update_log(f"💻 发出指令: {cmd}")
        except Exception:
            update_log("❌ 发送异常")
    else:
        update_log("⚠️ 设备未在线，无法发送")

def update_log(msg):
    txt_log.insert(tk.END, msg + "\n")
    txt_log.see(tk.END)

# ==========================================
# 🎨 GUI 现代极客界面布局
# ==========================================
window = tk.Tk()
window.title("商业级 IoT 全栈控制台 V2.0")
window.geometry("500x750")
window.configure(bg="#1a1a2e") # 更深邃的科技蓝背景

f_title = font.Font(family="Segoe UI", size=20, weight="bold")
f_data = font.Font(family="Consolas", size=14, weight="bold")
f_btn = font.Font(family="Segoe UI", size=12, weight="bold")

# 顶部状态栏（心跳指示灯）
frame_top = tk.Frame(window, bg="#16162b")
frame_top.pack(fill=tk.X, ipady=5)
lbl_status = tk.Label(frame_top, text="● 离线 (OFFLINE)", font=f_btn, fg="#e74c3c", bg="#16162b")
lbl_status.pack(side=tk.RIGHT, padx=15)
tk.Label(frame_top, text="💻 系统状态控制台", font=f_btn, fg="#95a5a6", bg="#16162b").pack(side=tk.LEFT, padx=15)

# 数据面板
frame_dashboard = tk.LabelFrame(window, text=" 📊 实时数据大屏 ", font=f_btn, fg="#e2e2e2", bg="#16162b", bd=1)
frame_dashboard.pack(pady=15, padx=20, fill=tk.X)

def add_row(parent, icon, text):
    f = tk.Frame(parent, bg="#16162b")
    f.pack(fill=tk.X, pady=4, padx=15)
    tk.Label(f, text=f"{icon} {text}", font=f_data, fg="#a4b0be", bg="#16162b").pack(side=tk.LEFT)
    l = tk.Label(f, text="--", font=f_data, fg="#eccc68", bg="#16162b")
    l.pack(side=tk.RIGHT)
    return l

lbl_temp = add_row(frame_dashboard, "🌡️", "环境温度:")
lbl_humi = add_row(frame_dashboard, "💧", "环境湿度:")
lbl_light = add_row(frame_dashboard, "☀️", "光照强度:")
lbl_led = add_row(frame_dashboard, "💡", "LED 状态:")
lbl_page = add_row(frame_dashboard, "📄", "当前页面:")

# 【图表组件区域】历史温度曲线
frame_chart = tk.LabelFrame(window, text=" 📈 历史温度趋势（最近30次采集） ", font=f_btn, fg="#e2e2e2", bg="#16162b", bd=1)
frame_chart.pack(pady=10, padx=20, fill=tk.X)
canvas = tk.Canvas(frame_chart, width=440, height=100, bg="#0f0f1a", highlightthickness=0)
canvas.pack(pady=10, padx=10)
draw_historical_chart() # 初始化空图表

# 隔空遥控器
frame_btns = tk.Frame(window, bg="#1a1a2e")
frame_btns.pack(pady=10)

tk.Button(frame_btns, text="🟢 无线开灯", font=f_btn, bg="#2ed573", fg="white", width=12, command=lambda: send_command("open")).grid(row=0, column=0, padx=15, pady=8)
tk.Button(frame_btns, text="🔴 无线关灯", font=f_btn, bg="#ff4757", fg="white", width=12, command=lambda: send_command("close")).grid(row=0, column=1, padx=15, pady=8)
tk.Button(frame_btns, text="🏠 切换首页", font=f_btn, bg="#1e90ff", fg="white", width=12, command=lambda: send_command("page1")).grid(row=1, column=0, padx=15, pady=8)
tk.Button(frame_btns, text="⚙️ 系统页面", font=f_btn, bg="#a442ff", fg="white", width=12, command=lambda: send_command("page2")).grid(row=1, column=1, padx=15, pady=8)

# 终端输出框
txt_log = tk.Text(window, height=6, bg="#000000", fg="#1dd1a1", font=("Consolas", 10))
txt_log.pack(fill=tk.BOTH, padx=20, pady=15, expand=True)

# 启动网络线程
threading.Thread(target=tcp_server_thread, daemon=True).start()
# 启动心跳超时监测雷达
window.after(1000, check_heartbeat_radar)

window.mainloop()