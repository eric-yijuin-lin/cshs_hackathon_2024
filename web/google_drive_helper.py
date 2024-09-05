import os.path
import threading
from time import sleep
from queue import Queue

from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
from googleapiclient.http import MediaFileUpload

# 建立全域變數 (函式外的變數，大家都可以用)
# 全大寫代表常數，除了「初始化」不會去改變它
SCOPES = ["https://www.googleapis.com/auth/drive.metadata.readonly",
          "https://www.googleapis.com/auth/drive.file"]
CRED_FILE = "./credentials/google-drive-cshs.json"
SERVICE = None
upload_queue = Queue(maxsize=100)

# 初始化 Google Drive 需要用的套件服務，然後存到 SERVICE 全域變數
# 雷：教學後面有提到要用瀏覽器登入，然後 token 會刷新
# 雷：登入認證畫面，權限記得打勾
creds = None
if os.path.exists(CRED_FILE):
  creds = Credentials.from_authorized_user_file(CRED_FILE, SCOPES)
# 如果金鑰檔案找不到或過期了，讓使用者重新登入並且更新金鑰檔
if not creds or not creds.valid:
  if creds and creds.expired and creds.refresh_token:
    creds.refresh(Request())
  else:
    flow = InstalledAppFlow.from_client_secrets_file(CRED_FILE, SCOPES)
    creds = flow.run_local_server(port=0)
  # Save the credentials for the next run
  with open(CRED_FILE, "w") as token:
    token.write(creds.to_json())
SERVICE = build("drive", "v3", credentials=creds)

def get_folder_id(folder_name):
  '''根據指定的名稱從雲端硬碟取得資料夾ID, 當作之後上傳的目標'''
  response = SERVICE.files().list(
    q=f"name='{folder_name}' and mimeType='application/vnd.google-apps.folder'",
    supportsAllDrives=True
  ).execute()

  items = response.get("files", [])
  if not items:
    print("No files found.")
    return None
  if len(items) > 1:
    print(f"Warning: more than 1 folders with name: '{folder_name}'!")
  return items[0]["id"]

def upload_to_folder(file_name, folder_id = ""):
  '''把圖片上傳到 folder_id 指定的資料夾, file_name 需要包含路徑、檔名'''
  try:
    file_metadata = {"name": os.path.basename(file_name), "parents": [folder_id]}
    media = MediaFileUpload(
        file_name, mimetype="image/jpeg", resumable=True
    )
    # pylint: disable=maybe-no-member
    file = (
        SERVICE.files()
        .create(body=file_metadata, media_body=media, fields="id")
        .execute()
    )
    return file.get("id")

  except HttpError as error:
    print(f"An error occurred: {error}")
    return None
  
def enqueue_image(file_name):
  if upload_queue.full() == True:
    print(f"warning: image queue is full, {file_name} is discarded")
  else:
    upload_queue.put(file_name)

def _upload_worker():
  while True:
    if not upload_queue:
      sleep(0.5)
      continue
    file_name = upload_queue.get()
    file_id = upload_to_folder(file_name)
    if file_id:
      print(f'File uploaded, file ID: "{file_id}".')
      os.remove(file_name)
    else:
      print("failed to upload image to google drive by threaded worker")
      print("file name:", file_name)

upload_thread = threading.Thread(target=_upload_worker)
