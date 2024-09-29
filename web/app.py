import os
from pathlib import Path
from datetime import datetime

from linebot.v3 import (
    WebhookHandler
)
from linebot.v3.exceptions import (
    InvalidSignatureError
)
from linebot.v3.messaging import (
    Configuration,
    ApiClient,
    MessagingApi,
    ReplyMessageRequest,
    PushMessageRequest,
    TextMessage
)
from linebot.v3.webhooks import (
    MessageEvent,
    FollowEvent,
    TextMessageContent,
)
import gspread
from flask import Flask, request, abort, render_template, flash

import google_drive_helper as drive_helper

ESP32_IMAGE_FOLDER = "./web/esp32-images/"


# # 啟動 google 試算表
service_account = gspread.service_account(filename = "./credentials/google-api-cshs.json")
work_book = service_account.open("竹山高中黑客松資料庫")
can_sheet = work_book.worksheet("垃圾桶狀態")

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = ESP32_IMAGE_FOLDER
app.config["MOVE_TO_DRIVE"] = False

if app.config["MOVE_TO_DRIVE"] == True:
    drive_helper.upload_thread.start()

line_config = Configuration(access_token="d+oEMT5F/gENUeRSF9yDeaGbGwLntnA5udkuBh+iIdWZK9KU4tCLoC0TfpH4EBRjF60nrsm5v3Wk6keR8VqfMI1eEGt7lp0KVz7sES+viEH7KkSzNhqSFLOT9He/yDLsTk8hfLg3cjpTTF2Wa1yaYQdB04t89/1O/w1cDnyilFU=")
hook_handler = WebhookHandler("8153e85b315f535d3ed0ba6fd0086124")

@app.route("/hello", methods=["GET"])
def hello():
    return "Hello"

@app.route("/esp32-upload", methods=["GET", "POST"])
def test_upload():
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
        file_name = f"{file_prefix} {time_str}{file_surffix}"
        full_name = os.path.join(app.config["UPLOAD_FOLDER"], file_name)
        file.save(full_name)

        move_to_google_drive = request.args.get("gd")
        if app.config["MOVE_TO_DRIVE"] == True:
            drive_helper.enqueue_image(full_name)
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

    cell = can_sheet.find(id)
    if cell:
        update_range = f"A{cell.row}:F{cell.row}"
        can_sheet.update(values=[data_row], range_name=update_range)
    else:
        can_sheet.insert_row(values=data_row, index=2)
    return "OK"

@app.route("/can-list")
def get_garbage_can_list():
    can_list = can_sheet.get_all_values()
    return can_list[1:] # 陣列從 0 開始

# ready: 待命, rotate: 旋轉, dump: 倒垃圾, resume: 回復角度, full: 滿了
@app.route("/get-status")
def get_can_status():
    id = request.args.get("id")
    cell = can_sheet.find(id)
    if cell:
        row_values = can_sheet.row_values(cell.row)
        status = row_values[5]
        return status
    else:
        return "id not found", 400

# ready: 待命, rotate: 旋轉, dump: 倒垃圾, resume: 回復角度, full: 滿了
@app.route("/update-status")
def update_can_status():
    id = request.args.get("id")
    status = request.args.get("s")
    cell = can_sheet.find(id)
    if cell:
        update_range = f"G{cell.row}"
        can_sheet.update_acell(update_range, status) # 指更新一個 cell
        return "OK"
    else:
        return "id not found", 400

# line 機器人的 http 請求 handler (處理函式)
@app.route("/callback", methods=['POST'])
def callback():
    # get X-Line-Signature header value
    signature = request.headers['X-Line-Signature']

    # get request body as text
    body = request.get_data(as_text=True)
    app.logger.info("Request body: " + body)

    # handle webhook body
    try:
        hook_handler.handle(body, signature)
    except InvalidSignatureError:
        app.logger.info("Invalid signature. Please check your channel access token/channel secret.")
        abort(400)

    return 'OK'

@app.route("/notify-clean", methods=['GET'])
def notify_garbage_clean():
    can_id = request.args.get("id")
    cell = can_sheet.find(can_id)
    row_values = can_sheet.row_values(cell.row)
    can_name = row_values[1]
    notify_id = row_values[4]

    with ApiClient(line_config) as api_client:
        messaging_api = MessagingApi(api_client)
        messaging_api.push_message(
            PushMessageRequest(
            to=notify_id,
            messages=[TextMessage(text=f"{can_name} 快要滿了，請協助清運")]
        ))

@hook_handler.add(FollowEvent)
def handle_follow(event):
    with ApiClient(line_config) as api_client:
        line_bot_api = MessagingApi(api_client)
        line_bot_api.reply_message(
            ReplyMessageRequest(
                reply_token=event.reply_token,
                messages=[TextMessage(text="歡迎加入竹山高中環境清理小幫手")]
            )
        )

if __name__ == "__main__":
    app.run(port=9002)
