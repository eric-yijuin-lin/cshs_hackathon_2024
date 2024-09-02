import os.path

import google.auth
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
from googleapiclient.http import MediaFileUpload


# If modifying these scopes, delete the file token.json.
SCOPES = ["https://www.googleapis.com/auth/drive.metadata.readonly",
          "https://www.googleapis.com/auth/drive.file"]
CRED_FILE = "./credentials/google-drive-cshs.json"

def get_drive_service():
  creds = None
  # The file token.json stores the user's access and refresh tokens, and is
  # created automatically when the authorization flow completes for the first
  # time.
  # 雷：教學後面有提到要用瀏覽器登入，然後 token 會刷新
  # 雷：登入認證畫面，權限記得打勾
  if os.path.exists(CRED_FILE):
    creds = Credentials.from_authorized_user_file(CRED_FILE, SCOPES)
  # If there are no (valid) credentials available, let the user log in.
  if not creds or not creds.valid:
    if creds and creds.expired and creds.refresh_token:
      creds.refresh(Request())
    else:
      flow = InstalledAppFlow.from_client_secrets_file(CRED_FILE, SCOPES)
      creds = flow.run_local_server(port=0)
    # Save the credentials for the next run
    with open(CRED_FILE, "w") as token:
      token.write(creds.to_json())
  service = build("drive", "v3", credentials=creds)
  return service
  
def get_folder_id(drive_service, folder_name):
  response = drive_service.files().list(
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

def list_files(drive_service, pageSize = 10):
  try:
    # Call the Drive v3 API
    results = (
        drive_service.files()
        .list(pageSize=pageSize, fields="nextPageToken, files(id, name)")
        .execute()
    )
    items = results.get("files", [])

    if not items:
      print("No files found.")
    else:
      print("Files:")
      for item in items:
        print(f"{item['name']} ({item['id']})")
  except HttpError as error:
    # TODO(developer) - Handle errors from drive API.
    print(f"An error occurred: {error}")

def create_folder(drive_service, folder_name):
  try:
    file_metadata = {
        "name": folder_name,
        "mimeType": "application/vnd.google-apps.folder",
    }

    # pylint: disable=maybe-no-member
    file = drive_service.files().create(body=file_metadata, fields="id").execute()
    print(f'Folder ID: "{file.get("id")}".')
    return file.get("id")

  except HttpError as error:
    print(f"An error occurred: {error}")
    return None


def upload_to_folder(drive_service, folder_id):
  try:
    file_metadata = {"name": "test.jpg", "parents": [folder_id]}
    media = MediaFileUpload(
        "./test/google_drive/test.jpg", mimetype="image/jpeg", resumable=True
    )
    # pylint: disable=maybe-no-member
    file = (
        drive_service.files()
        .create(body=file_metadata, media_body=media, fields="id")
        .execute()
    )
    print(f'File uploaded, file ID: "{file.get("id")}".')
    return file.get("id")

  except HttpError as error:
    print(f"An error occurred: {error}")
    return None

def main():
  drive_service = get_drive_service()
  folder_id = get_folder_id(drive_service, "跑班上傳區")
  upload_to_folder(drive_service, folder_id)

if __name__ == "__main__":
  main()