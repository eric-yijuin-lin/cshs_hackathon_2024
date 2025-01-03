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
from ai_model import predict_external_image

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
    
@app.route("/classify-garbage", methods=["GET", "POST"])
def classify_garbage():
    # # 檢查照片是否正確附在 http 請求裡
    if "file" not in request.files:
        return "No file part", 400
    file = request.files["file"]
    if file.filename == "":
        return "No selected file", 400
    
    # # 用時間戳當作檔案名稱，並且產生要存檔的檔案路徑
    file_prefix = Path(file.filename).stem
    file_surffix = Path(file.filename).suffix
    time_str = datetime.now().strftime("%Y-%m-%d %H%M%S")
    file_name = f"{file_prefix} {time_str}{file_surffix}"
    full_name = os.path.join(app.config["UPLOAD_FOLDER"], file_name)
    file.save(full_name)

    image_class = predict_external_image(full_name)
    print("garbage class:", image_class)
    return image_class



@app.route("/home", methods=["GET"])
def home():
    return render_template("index.html")

# https://goattl.tw/cshs/hackathon/update-can?id=CSHS1234&lat=23.76170563343541&lng=120.68148656873399&w=12.34&h=0.5678&time=1900-01-01
# 127.0.0.1:9002/update-can?id=CSHS1234&lat=23.76170563&lng=120.6814866&d1=22.90&d2=271.76&d3=278.39
@app.route("/update-can", methods=["GET"])
def update_garbage_can():
    can_id = request.args.get("id") # 垃圾桶 ID  
    lat = request.args.get("lat") # 緯度
    lng = request.args.get("lng") # 經度
    distance1 = request.args.get("d1") # 塑膠超音波距離 (用來推算容量)
    distance2 = request.args.get("d2") # 一般垃圾超音波距離 (用來推算容量)
    distance3 = request.args.get("d3") # 金屬超音波距離 (用來推算容量)
    date_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    data_row = [
        float(lat),
        float(lng),
        (100 - float(distance1)) / 100,
        (100 - float(distance2)) / 100,
        (100 - float(distance3)) / 100,
        date_time
    ]

    if data_row[2] < 0:
        data_row[2] = 0
    if data_row[3] < 0:
        data_row[3] = 0
    if data_row[4] < 0:
        data_row[4] = 0

    cell = can_sheet.find(can_id)
    if cell:
        update_range = f"C{cell.row}:H{cell.row}"
        can_sheet.update(values=[data_row], range_name=update_range)
    else:
        can_sheet.insert_row(values=data_row, index=2)
    return "OK"

@app.route("/can-list")
def get_garbage_can_list():
    can_list = can_sheet.get_all_values()
    return can_list[1:] # 陣列從 0 開始

# ready: 待命, rotate: 旋轉 N度, dump: 倒垃圾, resume: 回復旋轉角度, full: 滿了
@app.route("/get-command")
def get_can_command():
    id = request.args.get("id")
    cell = can_sheet.find(id)
    if cell:
        row_values = can_sheet.row_values(cell.row)
        command = row_values[8]
        return command
    else:
        return "id not found", 400

# ready: 待命, rotate: 旋轉 N度, dump: 倒垃圾, resume: 回復旋轉角度, full: 滿了
@app.route("/update-command")
def update_can_command():
    id = request.args.get("id")
    command = request.args.get("cmd")
    cell = can_sheet.find(id)
    if cell:
        update_range = f"I{cell.row}" # I1 
        can_sheet.update_acell(update_range, command) # 指更新一個 cell
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
    notify_id = row_values[9]
    print(type(notify_id))
    print(notify_id)

    with ApiClient(line_config) as api_client:
        messaging_api = MessagingApi(api_client)
        messaging_api.push_message(
            PushMessageRequest(
            to=notify_id,
            messages=[TextMessage(text=f"{can_name} 快要滿了，請協助清運")]
        ))
    return "OK"

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
    return "OK"

@hook_handler.add(MessageEvent, message=TextMessageContent)
def handle_text_message(event):
    with ApiClient(line_config) as api_client:
        line_bot_api = MessagingApi(api_client)
        user_id = event.source.user_id
        message = event.message.text
        reply_message = ""

        if message == "查詢垃圾":
            can_list = get_garbage_can_list()
            for can in can_list:
                if not can[0]:
                    continue
                reply_message += f"垃圾桶名稱:{can[1]}\n緯度:{can[2]}\n經度:{can[3]}\n塑膠容量:{can[4]}\n一般垃圾容量:{can[5]}\n金屬容量:{can[6]}\n最後更新:{can[7]}\n\n"
            line_bot_api.reply_message_with_http_info(
                ReplyMessageRequest(
                    reply_token=event.reply_token,
                    messages=[TextMessage(text=reply_message)]
                )
            )
    return "OK"

if __name__ == "__main__":
    app.run(port=9002)
