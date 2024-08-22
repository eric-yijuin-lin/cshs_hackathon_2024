import os
from pathlib import Path
from datetime import datetime

import gspread
# from ultralytics import YOLO
from flask import Flask, request, abort, render_template, flash

ESP32_IMAGE_FOLDER = "./web/esp32-images/"

# # 啟動 YOLO 模型
# yolo_model = YOLO("best.pt")

# # 啟動 google 試算表
service_account = gspread.service_account(filename = "./credentials/google-api.json")
work_book = service_account.open("竹山高中黑客松資料庫")
can_sheet = work_book.worksheet("垃圾桶狀態")
# print(os.getcwd())

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = ESP32_IMAGE_FOLDER
app.secret_key = "ASDFGH"

@app.route("/hello", methods=["GET"])
def hello():
    return "Hello"

@app.route("/test_upload", methods=["GET", "POST"])
def test_upload():
    print(request)
    print(os.getcwd())
    if request.method == "GET":
        return render_template("test_upload.html")
    elif request.method == "POST":
        if "file" not in request.files:
            flash("No file part")
            return 400, "No file part"
        file = request.files["file"]
        if file.filename == "":
            flash("No selected file")
            return 400, "No selected file"
        
        file_prefix = Path(file.filename).stem
        file_surffix = Path(file.filename).suffix
        time_str = datetime.now().strftime("%Y-%m-%d %H%M%S")
        file_to_save = f"{file_prefix} {time_str}{file_surffix}"
        file.save(os.path.join(app.config["UPLOAD_FOLDER"], file_to_save))
        return "image saved"

@app.route("/home", methods=["GET"])
def home():
    return render_template("index.html")

# https://goattl.tw/cshs/hackathon/update-can?id=CSHS1234&lat=23.76170563343541&lng=120.68148656873399&w=12.34&h=0.5678&time=1900-01-01
# 127.0.0.1:9002/update-can?id=CSHS1234&lat=23.76170563343541&lng=120.68148656873399&w=12.34&h=0.5678
@app.route("/update-can", methods=["GET"])
def update_garbage_can():
    can_id = request.args.get("id") # 垃圾桶 ID
    lat = request.args.get("lat") # 緯度
    lng = request.args.get("lng") # 經度
    weight = request.args.get("w") # 重量
    height = request.args.get("h") # 高度
    date_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    data_row = [
        can_id,
        float(lat),
        float(lng),
        float(weight),
        float(height),
        date_time
    ]

    cell = can_sheet.find("debug")
    if cell:
        print(cell.col, cell.row)
        update_range = f"A{cell.row}:F{cell.row}"
        can_sheet.update( values=[data_row], range_name=update_range)
    else:
        can_sheet.insert_row(values=data_row, index=2)
    return "OK"

@app.route("/can-list")
def get_garbage_can_list():
    can_list = can_sheet.get_all_values()
    return can_list[1:]

if __name__ == "__main__":
    app.run(port=9002)
