import os
from datetime import datetime

import gspread
from ultralytics import YOLO
from flask import Flask, request, abort, render_template, flash, redirect

ESP32_IMAGE_FOLDER = "./esp32-images"

# 啟動 YOLO 模型
yolo_model = YOLO("best.pt")

# 啟動 google 試算表
service_account = gspread.service_account(filename = "金鑰檔案.json")
work_book = service_account.open("試算表名稱")
log_sheet = work_book.worksheet("子表格名稱")

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = ESP32_IMAGE_FOLDER

@app.route("/hello", methods=["GET"])
def hello():
    return "Hello"

@app.route("/home", methods=["GET"])
def home():
    return render_template("index.html")

@app.route("/detect", methods=["POST"])
def detect_garbage():
    if request.method == "POST":
        # check if the post request has the file part
        if "file" not in request.files:
            flash("No file part")
            return 400, "No file part"
        file = request.files["file"]
        # if user does not select file, browser also
        # submit a empty part without filename
        if file.filename == "":
            flash("No selected file")
            return 400, "No selected file"
        
        time_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        file_to_save = file.filename.replace(".jpg", f"{time_str}.jpg")
        file.save(os.path.join(app.config["UPLOAD_FOLDER"], file_to_save))

        predict_result = yolo_model.predict(file_to_save)[0]
        boxes = predict_result.boxes  # Boxes object for bounding box outputs
        masks = predict_result.masks  # Masks object for segmentation masks outputs
        keypoints = predict_result.keypoints  # Keypoints object for pose outputs
        probs = predict_result.probs  # Probs object for classification outputs
        obb = predict_result.obb  # Oriented boxes object for OBB outputs
        predict_result.show()  # display to screen
        predict_result.save(filename=f"./yolo-images/detect{time_str}.jpg")  # save to disk

        robot_command = "pause"
        if boxes[0] > 0.55:
            robot_command = "left"
        elif boxes[0] < 0.45:
            robot_command = "right"
        elif boxes[1] > 0.55:
            robot_command = "up"
        elif boxes[2] < 0.45:
            robot_command = "down"
        
        robot_log = [time_str, "機器人ID", robot_command, "經度", "緯度"]
        
def insert_command_log(log_data: list):
    pass
    
if __name__ == "__main__":
    app.run(port=9002)
